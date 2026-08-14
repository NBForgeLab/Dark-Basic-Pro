# خطة الموجة 11 — تعليمات القوائم كبناءات داخلية (TDD)

التصميم: `docs/superpowers/specs/2026-08-11-x64-list-instructions-design.md`

- [x] Design doc: `docs/superpowers/specs/2026-08-11-x64-list-instructions-design.md`
- [x] RED: `tests/test_x64_list_instructions.cpp` — تراجع مؤقت إلى الاسم
      الداخلي `+list` أثبت فشل كل الاختبارات (اسم غير مطابق،
      `FindInstruction=false`، `MakeStatements=false` — 9/9 أحمر)
- [x] GREEN: `InstructionTable.h` — قيم `InternalInstruction` للقوائم
      (401–409 بعد `EndError=309`)
- [x] GREEN: `InstructionTable.cpp` — تسجيل 12 أمرًا عبر `AddCommandCore2`
      **بالأسماء الظاهرة** (`ARRAY INSERT AT TOP`...) وبأسماء x64 المزخرفة
      المتحقق منها عبر dumpbin (`_K`/`_KH`/`X_K`/`X_ KH` — بلا مسافة)
- [x] GREEN: `Parameter.cpp` — مسار `H` يفرض 1002 (كامل العرض) بدل 7
- [x] الاختبارات الكاملة + البناء الكامل + ctest — 1013/0/1، بناء نظيف،
      ctest 100%
- [x] توثيق الحالة في `docs/17` وتحديث صندوق الخطة

## اكتشافات TDD

1. **الاسم الجذري**: التسجيل الداخلي باسم `+list` لا يطابق نص المصدر أبدًا —
   تعليمات القوائم تُكتب في المصدر صراحةً، فلزمت الأسماء الظاهرة
   (`ARRAY INSERT AT TOP`...). النمط المثبت: `inc`/`dec` تُسجَّل بأسمائها
   الظاهرة، بينما `+allocate`/`+deallocate` أسماء داخلية تُستدعى عبر `GetRef`.
2. **الفوز على `.rc`**: القاعدة الداخلية تُحمَّل أولًا
   (`SetInternalInstructionDatabase` قبل `LoadInstructionDatabase`)، فتصبح
   إدخالات x64 رأس سلسلة الأصدقاء وتربح `ResolveEntry` على أسماء 32-bit
   القديمة في موارد DLL.
3. **`GetRef` vs `FindInstruction`**: `GetRef(InternalInstruction::X)` يحمل
   **آخر** صديق مُسجَّل (التحميل `HL`)، بينما `FindInstruction` يحلّ إلى
   **رأس** السلسلة (التحميل `H`). الاختبارات تعكس هذا.
4. **أسماء mangling**: لا مسافات في أسماء MSVC x64 — `YAX_ KH` خطأ، الصحيح
   `YAX_KH` (تحقق dumpbin تجريبيًا مع `vcvars`).
