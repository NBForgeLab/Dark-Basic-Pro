# تصميم الموجة 11 — تعليمات القوائم كبناءات داخلية (TDD)

التاريخ: 2026-08-11
المرجع: `docs/superpowers/plans/2026-08-11-x64-list-instructions.md`

## 1. الهدف

تسجيل تعليمات القوائم (`ARRAY INSERT AT TOP/BOTTOM/ELEMENT`، `ARRAY DELETE
ELEMENT`، `ADD TO QUEUE` / `REMOVE FROM QUEUE`، `ADD TO STACK` /
`REMOVE FROM STACK`، `EMPTY ARRAY`) في قاعدة الأوامر الداخلية للمُصرِّف
(`SetInternalInstructionDatabase`) بأسمائها المزخرفة x64 الكاملة (`uintptr_t`
= `_K` في mangling MSVC) بدل الاعتماد على أسماء 32-bit في موارد DLL
(`?ArrayInsertAtTop@@YAKK@Z`)، بحيث يُصدِر المُصرِّف استدعاءات للواجهة الموسّعة
من الموجة 7.

## 2. التشخيص

- واجهة وقت التشغيل (DBDLLCore.cpp) وُسِّعت في الموجة 7 إلى `uintptr_t`:
  `ArrayInsertAtTop(uintptr_t)` ← `?ArrayInsertAtTop@@YA_K_K@Z`،
  `ArrayInsertAtTop(uintptr_t, int)` ← `?ArrayInsertAtTop@@YA_KKH@Z`، إلخ.
- سلسلة موارد `DBDLLCore.rc` ما زالت تحمل الأسماء القديمة 32-bit
  (`?ArrayInsertAtTop@@YAKK@Z`) — مسار `LoadCommandsFromDLL` سيُصدِر استيرادًا
  قديمًا غير موجود في الـ DLL الموسّع.
- النمط المثبت (الموجة 7): `Alloc`/`Free` مُسجَّلان داخليًا بأسماء x64 عبر
  `AddCommandCore` ويمرّان بمعامل الصفيف كنوع **1002** (عرض كامل).
- مسار معامل `H` (مصفوفة كمدخل) في `Parameter.cpp` يفرض `m_dwType=7`
  (DWORD = 4 بايت) — بقايا 32-bit: على x64 يجب أن يكون **1002** (8 بايت)
  حتى يُدفَع مؤشر الصفيف كامل العرض.

## 3. التصميم

### 3.1 قيم `InternalInstruction` الجديدة (InstructionTable.h)

قيم جديدة بعد `EndError=309` (المدى ≤ 999 آمن):

- `ArrayInsertTop`, `ArrayInsertBottom`, `ArrayInsertElement`
- `ArrayDeleteElement`, `EmptyArray`
- `AddToQueue`, `RemoveFromQueue`, `PushStack`, `PopStack`

### 3.2 التسجيل الداخلي (InstructionTable.cpp — SetInternalInstructionDatabase)

نفس شكل `Alloc`/`Free`، عبر `AddCommandCore2` لضبط `bPassArrayAsInput`
و`dwPlace` (موضع `*` = 1: القيمة الراجعة تُكتب في معامل الصفيف):

| الأمر | المعاملات | الاسم x64 | resultp | pmax | place | passArray |
|---|---|---|---|---|---|---|
| ARRAY INSERT AT TOP | H | `?ArrayInsertAtTop@@YA_K_K@Z` | 7 | 1 | 1 | ✓ |
| ARRAY INSERT AT TOP | HL | `?ArrayInsertAtTop@@YA_K_KH@Z` | 7 | 2 | 1 | ✓ |
| ARRAY INSERT AT BOTTOM | H | `?ArrayInsertAtBottom@@YA_K_K@Z` | 7 | 1 | 1 | ✓ |
| ARRAY INSERT AT BOTTOM | HL | `?ArrayInsertAtBottom@@YA_K_KH@Z` | 7 | 2 | 1 | ✓ |
| ARRAY INSERT AT ELEMENT | HL | `?ArrayInsertAtElement@@YA_K_KH@Z` | 7 | 2 | 1 | ✓ |
| ARRAY DELETE ELEMENT | H | `?ArrayDeleteElement@@YAX_K@Z` | 0 | 1 | 0 | ✗ |
| ARRAY DELETE ELEMENT | HL | `?ArrayDeleteElement@@YAX_KH@Z` | 0 | 2 | 0 | ✗ |
| EMPTY ARRAY | H | `?EmptyArray@@YAX_K@Z` | 0 | 1 | 0 | ✗ |
| ADD TO QUEUE | H | `?AddToQueue@@YA_K_K@Z` | 7 | 1 | 1 | ✓ |
| REMOVE FROM QUEUE | H | `?RemoveFromQueue@@YAX_K@Z` | 0 | 1 | 0 | ✗ |
| ADD TO STACK | H | `?PushToStack@@YA_K_K@Z` | 7 | 1 | 1 | ✓ |
| REMOVE FROM STACK | H | `?PopFromStack@@YAX_K@Z` | 0 | 1 | 0 | ✗ |

### 3.3 إصلاح مسار `H` في Parameter.cpp

في `CastAllParametersToInstruction`، فرع `bForceParamToTheArrayAddress`:
تغيير `m_dwType=7` إلى `m_dwType=1002` — مؤشر الصفيف كامل العرض (نفس نوع
`DoAllocation`/`DoDeAllocation` في الموجة 7). هذا يُصلح كل أوامر `H`
(وليس القوائم فقط) على x64.

### 3.4 (اختياري) تحديث `DBDLLCore.rc`

تحديث أسماء `%...%` المزخرفة في موارد الـ DLL لتطابق x64 — نفس صف
الموجة 7 (المصدر وُسِّع، والـ DLL المُجمَّع المُوزَّع يبقى أثر نشر).

## 4. الاختبارات (TDD)

`tests/test_x64_list_instructions.cpp`:

1. **جدول**: `GetRef(InternalInstruction::X)->GetDecoratedName()` = كل اسم x64
   أعلاه.
2. **مُجمَّع**: `dim a(10)` + `array insert at top a(0), 5` + `end` يُنتج:
   - دفع مؤشر الصفيف كامل العرض (نمط `48 A1`/`48 8B 85` ثم `50`)،
   - استدعاء عبر `FF D3` (CALL RBX) في مسار x64،
   - تخزين RAX الراجع (المؤشر الجديد) في فتحة الصفيف بنمط `48 A3`/`48 89 85`.
3. **مُجمَّع**: `array delete element a(0)` (بلا قيمة راجعة) يستدعي
   `?ArrayDeleteElement@@YAX_K@Z` ولا يُخزِّن راجعًا.
4. **مُجمَّع**: `add to stack a(0)`/`remove from queue a(0)` يمرران المؤشر
   كامل العرض.

## 5. اكتشافات TDD (بعد التنفيذ)

1. **الاسم الجذري** — كانت التسجيلات باسم داخلي `+list`، لكن تعليمات القوائم
   تُكتب في المصدر صراحةً (`array insert at top a(0), 5`)، فلزمت **الأسماء
   الظاهرة** (`ARRAY INSERT AT TOP`...) لتطابقها `FindEntry` عبر البحث التدريجي
   عن أطول بادئة. النمط المثبت: `inc`/`dec`/`sync`/`end` تُسجَّل بأسمائها
   الظاهرة؛ `+allocate`/`+deallocate` أسماء داخلية تُستدعى عبر `GetRef`
   (لأن `dim`/`undim` كلمات محجوزة تُعالج في Statement.cpp مباشرة).
2. **الفوز على `.rc`** — القاعدة الداخلية تُحمَّل أولًا
   (`SetInternalInstructionDatabase` قبل `LoadInstructionDatabase` في
   DBPCompiler.cpp:147-148)، فتصبح إدخالات x64 **رأس** سلسلة الأصدقاء في
   `TDictionary::Lookup` (create-if-not-exist + إلحاق)، و`ResolveEntry` يُرجع
   أول صديق يمر بفحص نوع الراجع — أي الإدخال x64 الداخلي. إدخالات `.rc`
   32-bit تبقى أصدقاء لاحقين لا يُختارون أبدًا.
3. **`GetRef` vs `FindInstruction`** — `GetRef(InternalInstruction::X)` يحمل
   **آخر** تسجيل للقيمة الداخلية (التحميل `HL`، لأن `SetRef` يُستدعى لكل
   تسجيل فيُكتب فوق السابق)، بينما `FindInstruction` يحلّ إلى **رأس**
   السلسلة (التحميل `H`). الاختبارات تعكس هذا التمييز بدقة.
4. **أسماء mangling بلا مسافات** — `YAX_ KH` (بمسافة) اسم خاطئ؛ الصحيح
   `YAX_KH`. تحقق تجريبي عبر dumpbin على برنامج مُجمَّع بـ `vcvars` — خطأ
   المسافة كاد يُدخل اسمًا مزخرفًا مكسورًا في القاعدة.
5. **دلالات `H*`/`H`** — شكل `%H*%` (star): `bPassArrayPtrAsInput=true`،
   `dwPlace=dwStarPos=1` (الراجع يُكتب في معامل الصفيف)، `resultp=7`؛ شكل
   `%H%` (plain): `dwStarPos=0`، `cFirstParamChar=0`، `resultp=0`،
   `bPassArrayPtrAsInput=false`، `place=0`. التسجيلات الداخلية تطابق هذه
   الدلالات تمامًا في `TurnStringIntoCommand`.

## 6. المعايير

- لا تغيير في سلوك الإنتاج خارج مسار القوائم (التسجيل يضيف أوامر، لا يزيل).
- صفر استدعاءات 32-bit جديدة؛ كل مؤشر صفيف يمر بثمانية بايت.
- إصلاح `H`→1002 في Parameter.cpp يعالج كل أوامر `H` (وليس القوائم فقط).
