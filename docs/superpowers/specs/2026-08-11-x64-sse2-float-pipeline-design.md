# الموجة 8 — مجرى الأرقام العائمة x87 → SSE2 في المُصدِّر

**التاريخ**: 2026-08-11 · **النطاق**: المُصدِّر (ASMWriter/TaskEmitter/InstructionTable) · **الأسلوب**: TDD

## 1. النموذج الحالي — x87 ورفض الحساب المرمَّز

| النوع | المعنى | العرض | التحميل/التخزين |
|---|---|---|---|
| 2 | float | 4 بايت | EAX (نمط البتات) — لا x87 |
| 8 | double float | 8 بايت | **FLD/FSTP (ST0)** — أوبكود x87 |
| 9 | double integer (int64) | 8 بايت | EDX:EAX — خارج النطاق |

الاكتشافات الجوهرية بالفحص:
1. **أوبكود x87 الثمانية فقط** (MOVMEMST08/MOVST0MEM8/…/MOVST0EAX8) تُستخدم لتحميل/تخزين double عبر FPU stack (ST0)، في `TaskEmitter::WriteASMXtoEAX/WriteASMEAXtoX` (11 موضعًا).
2. **حساب الأعداد العائمة كلها عبر استدعاءات DLL** في وقت التشغيل: `?AddFFF`/`?MulOOO`/`?EqualLOO`/… (BuildID=0 في `InstructionTable.cpp`). المسار المرمَّز (ASMTask::Add/…) **يرفض type 8** (`ERR_SYNTAX+50`) ويعامل type 2 كـ DWORD (جمع نمط البتات = خطأ قائم).
3. **`inc a#` / `dec a#`**: `BuildTask::IncAdd/DecAdd` في `ParseInstruction.cpp` يدفع ويستدعي DLL — لا مسار مرمَّز.
4. **التحويلات int↔float** (`CastLtoO`, `CastOtoL`, `CastDtoF`, …) كلها DLL.

## 2. خريطة التحويل — SSE2 (XMM0 مجمّع، XMM1 المعامل الثاني)

### 2.1 امتداد جدول الأوبكود: حقل `modrm`

أوبكود SSE2 ذات الذاكرة تتطلب 4 بايت قبل الإزاحة (`F2 0F 10 modrm`)، بينما بنية `ASMOpcodeDef` الحالية (preOp/op1/op2) تعبّر عن 3. الإضافة النظيفة:
- `ASMOpcodeDef.modrm = -1` وحقل اختياري في `DefineASM`.
- `CreateASMMiddleCore`:
  - مسار `PtrIndirect`: يكتب preOp/op1/op2 كبايتات أوبكود ثم `(modrm & 0xF8) | 3` (rm→RBX) إن وُجد، وإلا السلوك القديم (op2 هو modrm).
  - المسار العام (Imm32): يكتب preOp/op1/op2 ثم modrm ثم فتحة الإزاحة.

### 2.2 تحويل أوبكود x87 الثمانية في مكانها (نفس قيم enum، أسماء جديدة)

| القديم (x87) | الجديد (SSE2) | الترميز |
|---|---|---|
| MOVST0MEM8  (FLD)   | MOVSDXMM0MEM   | F2 0F 10 03 |
| MOVMEMST08 (FSTP)   | MOVSDMEMXMM0   | F2 0F 11 03 |
| MOVST0EBP8          | MOVSDXMM0EBP   | F2 0F 10 85 <disp32> |
| MOVEBPST08          | MOVSDEBPXMM0   | F2 0F 11 85 <disp32> |
| MOVST0EAX8          | MOVSDXMM0EAX   | F2 0F 10 80 <disp32> |
| MOVEAXST08          | MOVSDEAXXMM0   | F2 0F 11 80 <disp32> |
| MOVST0ECXOFF8       | MOVSDXMM0ECXOFF| F2 0F 10 81 <disp32> |
| MOVECXOFFST08       | MOVSDECXOFFXMM0| F2 0F 11 81 <disp32> |

قيم enum ثابتة (41/42/44/45/94/95/108/109) → مواضع الاستدعاء الـ11 في TaskEmitter تتغير أسماءً فقط. سلوك نوع 8 في `WriteASMXtoEAX`/`WriteASMEAXtoX` يبقى نفسه: «حمّل/خزّن double في المجمّع» — المجمّع صار XMM0 بدل ST0، ومسار المكدس (دفع نصفي double) يعمل بلا تغيير.

### 2.3 أوبكود SSE2 الجديدة (قيم 253+)

ذاكرة MOVSS (float — لمسار الحساب الجديد):
`MOVSSXMM0MEM`/`MOVSSMEMXMM0` (F3 0F 10/11 03)، `MOVSSXMM0EBP`/`MOVSSEBPXMM0` (…85)، `MOVSSXMM0EAX`/`MOVSSEAXXMM0` (…80)، `MOVSSXMM0ECXOFF`/`MOVSSECXOFFXMM0` (…81).

تسجيل-تسجيل وعمليات:
- نسخ: `MOVSDXMM1XMM0` (F2 0F 10 C8)، `MOVSSXMM1XMM0` (F3 0F 10 C8).
- حساب: `ADDSD/SUBSD/MULSD/DIVSD XMM0,XMM1` (F2 0F 58/5C/59/5E C1)، وSS للأربعة (F3).
- مقارنة: `UCOMISD XMM0,XMM1` (66 0F 2E C1)، `UCOMISS` (0F 2E C1)، وبوابات جديدة SETA/SETAE/SETB/SETBE (0F 97/93/92/96) وSETP/SETNP (0F 9A/9B) و`AND AL,AH`/`OR AL,AH` (20 E0 / 08 E0) — لأن علمي UCOMISD كعلمي الطرح (CF=below, ZF=equal, PF=unordered) وليسا Signed.
- تحويلات: `MOVD XMM0,EAX` (66 0F 6E C0 — نمط بتات float)، `CVTSI2SD/CVTSI2SS XMM0,EAX` (F2/F3 0F 2A C0)، `CVTTSD2SI/CVTTSS2SI EAX,XMM0` (F2/F3 0F 2C C0)، `CVTSD2SS XMM0,XMM0` (F2 0F 5A C0)، `CVTSS2SD XMM0,XMM0` (F3 0F 5A C0).

### 2.4 مسارات الحساب المركّب في `WriteASMTaskCore`

**Add/Sub/Mul/Div** — فرع SSE2 قبل الرفض الحالي، لـ type ∈ {2, 102, 8, 108}:
```
تحميل B عبر WriteASMXtoEAX (يضع XMM0 للـdouble / EAX للـfloat)
  double: MOVSD XMM1,XMM0        float: MOVD XMM0,EAX; MOVSS XMM1,XMM0
تحميل A عبر WriteASMXtoEAX
  float: MOVD XMM0,EAX
ADDSD/SUBSD/MULSD/DIVSD (أو SS) XMM0,XMM1
تخزين النتيجة:
  double: WriteASMEAXtoX (يخزّن XMM0 بعد التحويل)
  float:  MOVSS [@$_TEMPA_],XMM0; MOVEAXMEM4 [@$_TEMPA_]; WriteASMEAXtoX (EAX)
```
المفتاح: تحميل B أولًا إلى XMM1 ثم A إلى XMM0 يحفظ ترتيب الطرح/القسمة، ويُعيد استخدام آليات التحميل/التخزين القائمة كاملة (كل ParamMode بما فيه المصفوفات عبر WriteASMARRtoEAX التي صارت MOVSD). المعامل IMM: يُكتَب نمط البتات في `@$_TEMPA_`/`@$_TEMPB_` (عبر `GetDWORDRepresentation` للموجودة في مسار IMM) ثم MOVSD/MOVSS من TEMPA.

**مقارنة (Equal/NotEqual/Greater/GreaterEqual/Less/LessEqual)** — فرع SSE2:
```
تحميل B → XMM1 (كأعلاه)، ثم A → XMM0
MOV EAX,0 ثم UCOMISD/UCOMISS XMM0,XMM1 ثم SETcc AL
  Equal:        SETE AL; SETNP AH; AND AL,AH      (ZF && !PF — صحيح مع NaN)
  NotEqual:     SETNE AL; SETP AH; OR AL,AH
  Greater:      SETA AL                            (CF=0 && ZF=0 — آمن مع NaN)
  GreaterEqual: SETAE AL
  Less:         SETB AL; SETNP AH; AND AL,AH
  LessEqual:    SETBE AL; SETNP AH; AND AL,AH
تخزين EAX عبر WriteASMEAXtoX (النتيجة integer type 1)
```

**IncVar/DecVar** على floats (يمسح `inc a#`/`dec a#` الخاطئ): MOVSD/MOVSS XMM0,[a]; MOV EAX,1; CVTSI2SD/CVTSI2SS XMM1,EAX; ADD/SUB; تخزين.

**Cast** (تحويلات int↔float في المُصدِّر) — BuildTask وASMTask جدد:
| Cast | التسلسل |
|---|---|
| int→float | WriteASMXtoEAX(P1); CVTSI2SS XMM0,EAX; تخزين float |
| int→double | WriteASMXtoEAX(P1); CVTSI2SD XMM0,EAX; تخزين double |
| float→int | WriteASMXtoEAX(P1); MOVD XMM0,EAX; CVTTSS2SI EAX,XMM0; تخزين EAX |
| float→double | WriteASMXtoEAX(P1); MOVD XMM0,EAX; CVTSS2SD XMM0,XMM0; تخزين double |
| double→int | WriteASMXtoEAX(P1); CVTTSD2SI EAX,XMM0; تخزين EAX |
| double→float | WriteASMXtoEAX(P1); CVTSD2SS XMM0,XMM0; تخزين float |

### 2.5 InstructionTable وParseInstruction

- إدخالات `+mathfloat` (FFF) و`+mathdoublef` (OOO): Add/Sub/Mul/Div والمقارنات الست → `AddBuildCommand` بـ BuildTask::Add/… (تبقى Power/Mod DLL).
- إدخالات `+cast`: `CastLToF/CastLToO/CastFTOL/CastFTOO/CastDTOF/CastDTOO/CastFTOD/CastOTOL/CastOTOF/CastOTOD` → BuildTask::CastIntToFloat/… الجدد.
- `ParseInstruction` — كتلة `IncAdd/DecAdd`: إذا أصبحت التعليمية build، مرّر عبر `WriteASMTaskCore(Add/Sub, pP1, pValue, pP1)` بدل الاستدعاء.
- `MathOp::WriteDBMBit` — إضافة تعيينات BuildTask الجدد (IncAdd/DecAdd + الست Cast) إلى ASMTask.

## 3. حدود النطاق (مؤجَّل/خارج)
- int64 (type 9/R) — حساب وتحويلاته تبقى DLL (موجة لاحقة).
- Power/Mod للأعداد العائمة تبقى DLL (لا تعليمات SSE2 مباشرة).
- لا تغيير في واجهة وقت التشغيل (dbprocore دوال AddFFF/… تبقى مصدَّرة للتوافق؛ المترجم فقط لا يستدعيها بعد الآن للمسارات المحوَّلة).

## 4. اكتشافات TDD (مؤكدة بالاختبارات الحمراء)

1. **أوبكود x87 كانت للـ double فقط**: تحويل الـ8 أوبكود في مكانها (نفس قيم enum 41/42/44/45/94/95/108/109) لم يُغيّر أي موضع استدعاء سلوكيًا — «حمّل/خزّن double في المجمّع» بقي كما هو والمجمّع صار XMM0.
2. **الفرق بين float وdouble في مسار الحساب**: double يُحمَّل مباشرة إلى XMM0 عبر `WriteASMXtoEAX` (بعد التحويل)، بينما float يبقى في EAX كنمط بتات → `MOVD XMM0,EAX` ثم نسخ XMM1. هذا أعاد استخدام كل آليات التحميل/التخزين القائمة (كل ParamMode بما فيه المصفوفات عبر `WriteASMARRtoEAX`).
3. **تخزين نتيجة float عبر الفتحة المؤقتة** `@$_TEMPA_` (MOVSS ثم إعادة تحميل EAX) أزال الحاجة إلى مسار تخزين موازٍ كامل — سطران فقط.
4. **`inc a#` كان يستدعي AddFFF عبر DLL**: بعد تحويل AddFFF إلى build، أصبحت كتلة `IncAdd/DecAdd` في ParseInstruction تستهلك اسم DLL فارغًا — أُصلح بتوجيه build إلى `WriteASMTaskCore(Add/Sub)` مع المعامل «1» من نوع float.
5. **خطأ قائم مكتشف**: `a#=1.5` (إسناد literal عشري لـ float) يتحطم في مسار DLL القديم (SEH 0xc0000005) — خارج نطاق الموجة 8 (مسار الإسناد، لا مسار البث).
6. **قيد قائم**: إعلان متغير عادي من نوع صريح (`dim d as float`) يفشل في pre-scan — يعمل فقط شكل المصفوفة (`dim a(5) as float`). اختبارات double حوِّلت إلى مستوى المهمة (WriteASMTaskCore مباشرة).
7. **اختبار جديد**: `test_x64_opcode_emission.cpp` كان يثبّت بايتات x87 (DD 1B/DD 03) → حُدِّث إلى عقد MOVSD (F2 0F 11 03 / F2 0F 10 03).
