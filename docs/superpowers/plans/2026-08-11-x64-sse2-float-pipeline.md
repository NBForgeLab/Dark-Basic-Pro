# خطة الموجة 8 — مجرى الأرقام العائمة x87 → SSE2 (TDD)

- [x] وثيقة التصميم: `docs/superpowers/specs/2026-08-11-x64-sse2-float-pipeline-design.md`
- [x] RED: `tests/test_x64_sse2_math.cpp` — اختبارات بايتًا ببايت (25/25 فاشلة على الكود القديم):
  - تحميل/تخزين double (MOVSD عبر RBX/RBP/RAX/RCX) وfloat (MOVSS) لكل صيغ الذاكرة
  - أوبكود الحساب: ADDSD/SUBSD/MULSD/DIVSD + ADDSS/SUBSS/MULSS/DIVSS
  - المقارنة: UCOMISD/UCOMISS + بوابات SETA/…/SETP/SETNP وتسلسلات NaN الآمنة
  - التحويلات: CVTSI2SD/CVTSI2SS/CVTTSD2SI/CVTTSS2SI/CVTSD2SS/CVTSS2SD + MOVD
  - مسار مركَّب على مستوى المترجم: `a# = b# + c#` (float) ومسارات double عبر المهمة مباشرة
- [x] GREEN: حقل `modrm` في `ASMOpcodeDef`/`DefineASM`/`CreateASMMiddleCore`
- [x] GREEN: تحويل أوبكود x87 الثمانية في مكانها (نفس قيم enum) + إعادة تسمية المواضع الـ11 في TaskEmitter
- [x] GREEN: أوبكود SSE2 الجديدة (ذاكرة MOVSS + تسجيل-تسجيل + مقارنة + تحويلات)
- [x] GREEN: فرعا SSE2 في `WriteASMTaskCore` (Add/Sub/Mul/Div + المقارنات)
- [x] GREEN: ASMTask/BuildTask الجدد للتحويلات + `WriteASMTaskCore` cast + `MathOp::WriteDBMBit`
- [x] GREEN: `InstructionTable.cpp` (float/double math + casts → builds) + كتلة `IncAdd/DecAdd` في `ParseInstruction`
- [x] تحديث تثبيت x87 القديم في `test_x64_opcode_emission.cpp`
- [x] المجموعة الكاملة (994/0/1) + البناء الكامل + مصفوفة الملحقات + ctest 100% + docs/17
