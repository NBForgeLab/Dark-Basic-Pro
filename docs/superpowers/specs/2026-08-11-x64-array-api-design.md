# الموجة 7 — واجهة مؤشر الصفيف x64: عناوين كاملة من وقت التشغيل إلى فتحة الصفيف

**التاريخ**: 2026-08-11 · **النطاق**: واجهة دوال الصفيف في DBDLLCore + المترجم (أسماء مزخرفة + تخزين RAX كامل) · **الأسلوب**: TDD

## 1. النموذج الحالي — 4 حدود اقتطاع (مؤكدة بمسبار على `dim a(10)`)

```
[وقت التشغيل] DARKSDK DWORD CreateArray(...) → return (DWORD)pArrayPtr   ← اقتطاع 1
[المترجم]     Push أول معامل (المؤشر القديم) عبر type 7 → MOVEAXMEM4 (A1) ← اقتطاع 2
[المترجم]     استدعاء DimDDD بأسمه المزخرف 32-bit                        ← عقد 3
[المترجم]     تخزين القيمة الراجعة عبر Assign type 7 → MOVMEMEAX4 (A3)   ← اقتطاع 4
```

الموجات 5 و6 وسّعت فتحات varspace (8 بايت) والتحميل عند وصول العنصر (SIB ×8 وجدول ref 8 بايت)، لكن **قيمة مؤشر الصفيف نفسها** ما زالت تقتطع عند حدود الـAPI ودفع/تخزين type 7 — فأي تخصيص فوق 4GB يُفسد العنوان. المسبار (السلاسل الفعلية):
- دفع المؤشر القديم: `A1 moffs` (4 بايت) ثم `PUSH` (سلوت 8).
- التخزين الراجع: `A3 moffs` (4 بايت).

## 2. خريطة التحويل

### 2.1 وقت التشغيل — `uintptr_t` لكل دوال الصفيف (DBDLLCore.cpp)

كل معاملات/قيم رجوع المؤشر في 33 دالة تصبح `uintptr_t`:
`CreateArray`، `FreeStringsFromArray`، `DeleteArray`، `ExpandArray`، `ClearDataBlock`،
`DimCore`، `ReDimCore`، `DimDDD`، `UnDimDD`، `ArrayIndexToBottom/Top`،
`NextArrayIndex`، `PreviousArrayIndex`، `ArrayIndexValid`، `ArrayCount`،
`ArrayInsertAtBottom` (نسختان)، `ArrayInsertAtTop` (نسختان)، `ArrayInsertAtElement`،
`ArrayDeleteElement` (نسختان)، `EmptyArray`، `PushToStack`، `PopFromStack`،
`AddToQueue`، `RemoveFromQueue`، `ArrayIndexToStack`، `ArrayIndexToQueue`،
`SaveArray`، `LoadArrayCore`، `LoadArray`، `GetArrayType`، والمساعد `IsArraySingleDim`.
(لا استدعاءات عابرة للملفات — كلها داخل DBDLLCore.cpp، والتغيير ميكانيكي.)

### 2.2 الأسماء المزخرفة الجديدة (نمط `_K` المؤكد في الموجة 5)

- `uintptr_t DimDDD(uintptr_t, DWORD×10)` → `?DimDDD@@YA_K_KKKKKKKKKK@Z`
- `uintptr_t UnDimDD(uintptr_t)` → `?UnDimDD@@YA_K_K@Z`

المواضع في المترجم:
- `InstructionTable.cpp` — `+allocate` و`+deallocate`.
- `CoreRuntimeApi.cpp` — `RESOLVE_REQUIRED(unDim, ...)` + typedef `CoreVoidDwordPointer` → نوع يطابق `uintptr_t`.
- `RuntimeBundleResolver.cpp` — `lifecycleExports`.
- `EXEBlock.cpp` — استدعاء `g_CORE_UnDim(pMemoryAllocation)` (مؤشر DWORD* بعرض 8 — متوافق ABI مع `uintptr_t` في RCX).

### 2.3 المترجم — نوع DBM جديد `1002` = "قيمة مؤشر كاملة العرض"

type 7 يُستخدم للمعاملات/القيم DWORD (أحجام الصفائف، فهارس CalcArrayOffset،
التحويلات) — **يجب أن تبقى 4 بايت**. القيمة الوحيدة التي تحتاج 8 بايت عند
Mem/دفع/تخزين هي **مؤشر الصفيف**. الحل الجذري: نوع DBM مخصص `1002`:

- `DetermineParamMode`: 1002 خارج المجموعات الخاصة (1001، 10X، 20X) → سلوك
  `@`/`: ` عادي تمامًا كـ 7 (Mem/Ebp).
- `DetermineASMCall`: `case 1002` → نفس متغيرات QWORD من الموجة 5
  (`MOVEAXMEM8`/`MOVMEMEAX8`/`MOVEAXEBP8`/`MOVEBPEAX8`/`MOVEAXECXOFF8`/
  `MOVECXOFFEAX8`/`MOVEAXECXREL8`/`MOVEAXEAXREL8`).
- `Statement.cpp` `DoAllocation`: فرض نوع المعامل الأول `7 → 1002`.
  `DoDeAllocation`: `SetResultType(7) → 1002`.
- كل عدادات الدفع/التنظيف: 1002 خارج {8,9,108,109} → سلوت واحد كـ 7 ✓؛
  وليس 3/13/103 → لا مسار نص خاص ✓؛ `resultp` يبقى 7 (لا StrFree) ✓.

السلاسل الناتجة لـ`dim a(10)`:
- دفع المؤشر القديم: `48 A1 moffs` (8 بايت) ثم PUSH.
- تخزين القيمة الراجعة: `48 A3 moffs` (8 بايت كامل في فتحة varspace).
- `undim a`: `48 A1` للمعامل + `48 A3` لمسح الفتحة (قيمة رجوع UnDimDD = 0).

### 2.4 اكتشافات TDD أثناء التنفيذ

- **تخزين undim الراجع كان مفقودًا أصلًا**: `DoDeAllocation` كان يضبط
  `SetReturnParameter("&a")` — الرمز `&a` يذهب إلى `ParamMode::Imm` في
  `WriteASMEAXtoX` فلا يُصدَر شيء إطلاقًا (لا مسح للفتحة ولا عقد 32-bit —
  لا شيء). أُزيل `SetReturnParameter` فيمُرّ التخزين عبر المعامل الأول
  (الرمز `@a`، النوع 1002) → `48 A3` — نفس آلية تخزين DIM تمامًا.
- **تحويلات C++**: `return pArrayPtr;` (LPSTR→uintptr_t) ليس ضمنيًا في C++ —
  تطلّب `(uintptr_t)`؛ و`g_CORE_UnDim` غيّر typedef إلى `void(*)(uintptr_t)`
  مع توسعة `pMemoryAllocation` عند موضع الاستدعاء في `EXEBlock.cpp`.
- **تحذير C4311** في `DBDLLCore.cpp:5556` (إعادة تفسير مؤشر إلى DWORD في
  مسار GetTypePattern) — قديم غير مرتبط، بقي كما هو.

### 2.5 قرارات مؤجّلة (خارج الموجة 7)

- بقية تعليمات القوائم (ArrayInsert/Queue/Stack...) ليست في InstructionTable
  — توقيعات وقت التشغيل توسَّعت هنا استباقيًا، وعند إضافة تعليماتها تُضاف
  أسماؤها المزخرفة `_K`.
- `&var` في كود المستخدم (قيمة عنوان متغير) تبقى type 7 (4 بايت) — استخدام نادر.
- x87→SSE2، فتحات label، سلامة `_ESP_`.

## 3. التحقق

- اختبارات جديدة `tests/test_x64_array_api.cpp` (6):
  - أسماء Alloc/Free المزخرفة الجديدة في InstructionTable.
  - إصدار type 1002: تحميل/تخزين 8 بايت (48 A1/48 A3/48 8B 85/48 89 85) + دفع.
  - على مستوى المترجم: `dim a(10)` — سلسلة كاملة بدفع 8 بايت واستدعاء
    `?DimDDD@@YA_K_KKKKKKKKKK@Z` وتخزين `48 A3` بعد `ADD RSP,0x58`؛
    و`dim+undim` — عدّاد `48 A3` = 2 (تخزين DIM + مسح UNDIM).
- تحديث تثبيتات الأسماء القديمة: `test_core_runtime_api.cpp`،
  `test_runtime_bundle.cpp` → `?UnDimDD@@YA_K_K@Z`.
- وقت التشغيل: إعادة تجميع `dbp_plugin_core` قسريًا (DBDLLCore.cpp) نظيفة؛
  مصفوفة الملحقات الـ18 نظيفة.
- **النتيجة**: 969 نجاحًا / 0 فشل / 1 تخطٍّ متوقع؛ البناء الكامل نظيف؛ ctest 100%.
