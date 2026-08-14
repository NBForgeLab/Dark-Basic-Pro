# خطة الموجة 12 — فك اقتران `CMathOp::IsLiteral` عن `g_pDBPCompiler` (TDD)

التاريخ: 2026-08-11
المرجع: `docs/superpowers/specs/2026-08-11-x64-literal-preference-decoupling-design.md`

## الصناديق

- [x] Design doc: `docs/superpowers/specs/2026-08-11-x64-literal-preference-decoupling-design.md`
- [x] RED: `tests/test_x64_literal_preference.cpp` — وحدة `IsLiteral` (true→8 /
      false→2 / integer→1)، وحدة `DoValue`، استقلال عن `g_pDBPCompiler=nullptr`،
      وتكاملي عبر `MakeStatements(..., true)` يصدّر MOVSD بدل mov eax 1.5f
- [x] GREEN: تنفيذ سلسلة المعاملات
      - `CStatementList`: عضو `m_bDoubleLiterals` + getter/setter + معامل
        `MakeStatements`/`AddMiniStatements`
      - `CDBPCompiler::MakeProgram`: تمرير `m_bDoubleLiterals`
      - `MathOp.cpp`: توقيعات `IsLiteral`/`DoValue`/`DoCastOnMathOp`/
        `DoValueComplexVariable`/`DoValueSingleVariable` + تمرير داخلي
      - `Statement.h`/`Statement.cpp`: توقيع `DoExpression` + نقاط الالتقاط
        (9 مستدعي DoExpression + DoDeclaration + 4 مستدعي DoCastOnMathOp +
        موضع IsLiteral في CASE)
      - `Parameter.cpp:429`: تمرير `g_pStatementList->GetDoubleLiterals()`
- [x] GREEN: تشغيل اختبارات الموجة 12 + المجموعة الكاملة (لا انحدارات)
- [x] البناء الكامل + ctest + التوثيق (docs/17 + تحديث صندوق الخطة)

## اكتشافات TDD (بعد التنفيذ)

1. موضع `IsLiteral` إضافي في معالجة `CASE` (Statement.cpp:1276) خارج جرد
   التصميم الأولي — كشفه فشل ترجمة الاختبارات التكاملية.
2. `test_statement_expression.cpp` و`test_mathop_expression.cpp` يستدعيان
   `DoExpression`/`DoValue`/`DoValueComplexVariable`/`DoValueSingleVariable`
   مباشرة — حُدِّثت لتوقيعات المعامل الجديدة.
3. المسار التكاملي يؤكد وصول التفضيل حتى المُصدِّر: `MakeStatements(..., true)`
   يصدّر `F2 0F 10` (MOVSD) بدل `B8 00 00 C0 3F` (mov eax,1.5f).

## النتائج

- 8 اختبارات جديدة في `test_x64_literal_preference.cpp` — كلها خضراء.
- المجموعة الكاملة: 1022 اختبارًا، 1021 نجاحًا / 0 فشل / 1 تخطٍّ متوقع.
- البناء الكامل نظيف؛ ctest `-C Debug` 100%.
