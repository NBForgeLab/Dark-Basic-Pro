# تصميم الموجة 12 — فك اقتران `CMathOp::IsLiteral` عن `g_pDBPCompiler` (TDD)

التاريخ: 2026-08-11
المرجع: `docs/superpowers/plans/2026-08-11-x64-literal-preference-decoupling.md`

## 1. الهدف

إزالة القراءة من الكائن العام `g_pDBPCompiler->m_bDoubleLiterals` داخل
`CMathOp::IsLiteral` نهائيًا، وتمرير تفضيل الـliteral (`bDoubleLiterals`)
**كمعامل عبر سلسلة الاستدعاء** من نقطة دخول التحليل (`MakeStatements`) وصولًا
إلى `IsLiteral`. النتيجة: `IsLiteral` و`DoValue` تصبحان نقيتين من أي كائن عام
(تعملان بمعاملاتهما فقط)، والقاعدة الوحيدة المتبقية لقراءة سياق التحليل تكون
في طبقة `CStatement::DoExpression`/`DoDeclaration` من `g_pStatementList`
(سياق التحليل القياسي المضمون الوجود — لا `g_pDBPCompiler`).

## 2. التشخيص

- `CMathOp::IsLiteral` (MathOp.cpp:2087) تقرأ حاليًا:
  ```cpp
  const bool bDoubleLiterals =
      g_pDBPCompiler != nullptr && g_pDBPCompiler->m_bDoubleLiterals;
  ```
  هذا اقتران خفي بدالة تحليل نقية؛ `g_pDBPCompiler` قد يكون `nullptr` في
  الاختبارات/المضيفين المضمّنين (لهذا وُجد حارس null أصلاً — ترقيع على أثر
  الاقتران، وليس حلاً).
- `IsLiteral` مستدعاة من موضعين فقط، كلاهما داخل `CMathOp::DoValue`
  (MathOp.cpp:206 و250) — السلسلة محصورة ويمكن تمرير المعامل عبر توقيعاتها.
- `DoValue` تُستدعى من خارج MathOp.cpp في موضعين فقط: `CStatement::DoExpression`
  (Statement.cpp:3950) و`DoDeclaration` (Statement.cpp:2141)، إضافة إلى
  الاستدعاءات الداخلية (376/388/648) وداخل `DoCastOnMathOp` (740) و
  `DoValueComplexVariable` (1451) و`DoValueSingleVariable` (1670).
- `DoCastOnMathOp` تُستدعى أيضًا من Parameter.cpp:429 وStatement.cpp:
  1098/2703/3058/3070 — كلها داخل سياق تحليل (`g_pStatementList` متاح).
- `m_bDoubleLiterals` في `CDBPCompiler` لا يُعيَّن `true` في أي مكان حاليًا
  (خيار CLI بلا مستخدم حي) — التغيير سلوكي محايد في الإنتاج لكنه يفتح الباب
  لتفعيله عبر معامل صريح من الاختبارات والمضيفين.

## 3. التصميم

### 3.1 `CStatementList` يحمل تفضيل الـliteral (سياق التحليل)

- عضو جديد `bool m_bDoubleLiterals = false;` + `SetDoubleLiterals(bool)` /
  `GetDoubleLiterals()`.
- `MakeStatements(LPSTR pData, DWORD Size, bool bDoubleLiterals = false)`
  يخزّن التفضيل (معامل افتراضي يحافظ على 98 استدعاء اختبار قائمًا).
- `AddMiniStatements(LPSTR pData, DWORD Size, bool bDoubleLiterals = false)`
  بالمثل (مسار التحليل الموازي في `MakeProgram`).

### 3.2 `CDBPCompiler::MakeProgram` يمرر الخيار من الكائن

```cpp
g_pStatementList->MakeStatements(m_pFileData, m_FileDataSize, m_bDoubleLiterals);
g_pStatementList->AddMiniStatements(pMiniData.get(), dwMiniSize, m_bDoubleLiterals);
```

القيمة الجذرية تبقى في `m_bDoubleLiterals` (خيار CLI) لكنها **تُمرَّر كمعامل**
عبر حدود المكونات بدل قراءتها من العمق.

### 3.3 سلسلة المعاملات في MathOp

| الدالة | التوقيع الجديد |
|---|---|
| `CMathOp::IsLiteral` | `(CStr*, DWORD*, bool bDoubleLiterals)` |
| `CMathOp::DoValue` | `(CStr*, bool bDoubleLiterals)` |
| `CMathOp::DoCastOnMathOp` | `(unique_ptr<CMathOp>&, DWORD, bool bDoubleLiterals)` |
| `CMathOp::DoValueComplexVariable` | `(CStr*, bool bDoubleLiterals)` |
| `CMathOp::DoValueSingleVariable` | `(CStr*, bool bDoubleLiterals)` |

- `DoValue` يمرر المعامل إلى `IsLiteral` (206, 250)، وإلى الاستدعاءات الداخلية
  (376, 388, 648)، وإلى `DoCastOnMathOp` (506/518/541/553/581/598)، وإلى
  `DoValueSingleVariable` (272) و`DoValueComplexVariable` (280).
- `DoCastOnMathOp` يمرره إلى `DoValue(&pTypeCodeStr)` (740).
- `DoValueComplexVariable` يمرره إلى `DoValue(pOneSubscript)` (1451).
- `DoValueSingleVariable` يمرره إلى `DoValue(&localVar)` (1670).
- `IsLiteral` يستخدم المعامل مباشرة، ويُحذف سطر قراءة `g_pDBPCompiler`
  والإعلان `extern CDBPCompiler* g_pDBPCompiler` من MathOp.cpp إن لم يعد
  مستخدمًا.

### 3.4 نقاط الالتقاط في طبقة CStatement

- `CStatement::DoExpression(CStr*, CParameter*, bool bDoubleLiterals)` — توقيع
  جديد، يمرر إلى `DoValue`.
- المستدعون الداخليون لـ `DoExpression` (9 مواضع داخل Statement.cpp) يمررون
  `g_pStatementList->GetDoubleLiterals()` — قراءة واحدة من سياق التحليل عند
  حافة الدخول، لا من العمق.
- `DoDeclaration` (Statement.cpp:2141): `pMathOp->DoValue(&varInitName,
  g_pStatementList->GetDoubleLiterals())`.
- مستدعو `DoCastOnMathOp` الخارجيون: Parameter.cpp:429 وStatement.cpp:
  1098/2703/3058/3070 يمررون `g_pStatementList->GetDoubleLiterals()`.

## 4. الاختبارات (TDD)

`tests/test_x64_literal_preference.cpp`:

1. **وحدة — `IsLiteral`**: `IsLiteral("1.5", &type, true)` → type **8**
   (double)؛ `IsLiteral("1.5", &type, false)` → type **2** (float)؛
   `IsLiteral("42", &type, true)` → type **1** (الأعداد الصحيحة لا تتأثر).
2. **وحدة — `DoValue`**: `DoValue("1.5", true)` → النتيجة type 8؛
   `DoValue("1.5", false)` → type 2 — يثبت أن المعامل يمر عبر `DoValue` نفسه.
3. **استقلال عن `g_pDBPCompiler`**: ضبط `g_pDBPCompiler = nullptr` ثم استدعاء
   `IsLiteral`/`DoValue` — لا انهيار (لا قراءة من الكائن العام إطلاقًا).
4. **تكاملي عبر `MakeStatements(prog, size, true)`**: برنامج
   `dim d as float\r\nd=1.5\r\nend\r\n` مع `bDoubleLiterals=true` يجب أن يُصدِّر
   مسار double (`F2 0F 10` MOVSD) بدل `mov eax,1.5f`؛ ومع `false` يبقى
   `B8 00 00 C0 3F` (mov eax 1.5f) كما في الموجة 10.

## 5. اكتشافات TDD (بعد التنفيذ)

1. **موضع `IsLiteral` إضافي خارج نطاق الجرد الأولي** — معالجة `CASE` في
   Statement.cpp:1276 يستدعي `IsLiteral` مباشرة (وليس عبر `DoValue`)؛ كشفه
   فشل الترجمة في الاختبارات التكاملية. أُضيف تمرير
   `g_pStatementList->GetDoubleLiterals()` هناك.
2. **`test_statement_expression.cpp` يستدعي `DoExpression` مباشرة** — ثلاثة
   مواضع تحتاج المعامل الجديد (لازم تحديثها مع تغيير التوقيع).
3. **مسار التصدير في الاختبار التكاملي** — مع `bDoubleLiterals=true`، البرنامج
   `dim d as float\r\nd=1.5\r\nend` يصدّر `F2 0F 10` (MOVSD XMM0, m64) بدل
   `B8 00 00 C0 3F` (mov eax,1.5f) — يؤكد أن التفضيل يصل حتى المُصدِّر عبر
   السلسلة كاملة، لا يقتصر على `IsLiteral`.

## 6. المعايير

- `IsLiteral` و`DoValue` بلا أي قراءة من كائن عام — معاملاتهما فقط.
- القاعدة الوحيدة لقراءة السياق: `g_pStatementList->GetDoubleLiterals()` في
  طبقة `CStatement`/`CParameter` (سياق التحليل القياسي، مضمون الوجود في كل
  مسارات التحليل بما فيها الاختبارات).
- صفر تغيير في سلوك الإنتاج الافتراضي (`m_bDoubleLiterals=false`).
- كل استدعاءات `DoValue`/`IsLiteral`/`DoCastOnMathOp`/`DoValueComplexVariable`/
  `DoValueSingleVariable` تُحدَّث — لا قيم افتراضية مخفية في العمق.
