# DarkBasic Pro Compiler: Comprehensive Legacy Codebase Audit & Technical Debt Catalog

> **وثيقة تدقيق تقني شامل لملفات ومكونات الكومبايلر:** حصر مفصل لكافة أنماط الكود القديم (Legacy Patterns)، وإرث معمارية 32-بت، والفئات الأحادية العملاقة (God Classes)، والحالة العامة المشتركة (Global Mutable State)، وتحديد ما يحتاجه كل ملف للانتقال إلى معايير C++20 / x64 الحديثة.

---

## 📋 فهرس المحتويات
1. [ملخص نتائج التدقيق والتحليل بالأرقام](#1-ملخص-نتائج-التدقيق-والتحليل-بالأرقام)
2. [التدقيق التفصيلي لمكونات الكومبايلر (ملفاً بملف)](#2-التدقيق-التفصيلي-لمكونات-الكومبايلر-ملفاً-بملف)
   - [2.1 الفئات الأحادية العملاقة (God Classes)](#21-الفئات-الأحادية-العملاقة-god-classes)
   - [2.2 إدارة وتحليل النصوص والسلاسل (String Handling)](#22-إدارة-وتحليل-النصوص-والسلاسل-string-handling)
   - [2.3 جداول الرموز والبيانات (Symbol & Data Tables)](#23-جداول-الرموز-والبيانات-symbol--data-tables)
   - [2.4 ملفات تحليل الجمل والتحكم (Parsing Modules)](#24-ملفات-تحليل-الجمل-والتحكم-parsing-modules)
   - [2.5 إدارة الذاكرة وتوليد الملفات التنفيذية (EXE Generation & PE Loading)](#25-إدارة-الذاكرة-وتوليد-الملفات-التنفيذية-exe-generation--pe-loading)
   - [2.6 واجهة الربط القديمة والإضافات (DLL Boundary & .rc Binding)](#26-واجهة-الربط-القديمة-والإضافات-dll-boundary---rc-binding)
3. [كتالوج الأنماط القديمة المطلوب استئصالها](#3-كتالوج-الأنماط-القديمة-المطلوب-استئصالها)
4. [مصفوفة ملفات الكومبايلر والحالة الفنية لكل منها](#4-مصفوفة-ملفات-الكومبايلر-والحالة-الفنية-لكل-منها)

---

## 1. ملخص نتائج التدقيق والتحليل بالأرقام

| المعيار الفني | الحالة الحالية في الكود | المعيار الهدف (C++20 / x64) |
|---|---|---|
| **أنواع Win32 القديمة (`DWORD`)** | **3,441 سطر** في الكومبايلر و **9,741 سطر** في SDK | استبدال بـ `std::size_t`, `uint32_t`, `uint64_t`, `uintptr_t` حسب الغرض |
| **المتغيرات العامة (`extern`)** | **أكثر من 160 متغيراً عاماً** مشتركاً | كبسلة كاملة داخل كائن سياق الترجمة `CompilerContext` |
| **إدارة الذاكرة والمؤشرات** | استخدام مؤشرات خام `T*`، `delete` يدوي، و `new[]` | ملكية صريحة عبر `std::unique_ptr`، `std::shared_ptr`، ومجموعات STL |
| **معالجة النصوص** | كلاس قديم [`CStr`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Str.h) يعود لعام 2001 مع `LPSTR` | `std::string`، `std::string_view`، `std::span<const char>` |
| **توليد كود التجميع (Backend)** | مصفوفات ثنائية ثابتة بحجم 300 وترقيع REX | مولد كود x86-64 حديث مبني على تمثيل وسيط (Typed IR) |
| **أكبر الملفات حجماً** | [`Statement.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Statement.cpp) (5,258 سطر)، [`ASMWriter.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/ASMWriter.cpp) (2,997 سطر) | تفكيك إلى وحدات ومحللات صغيرة متخصصة ذات مسؤولية واحدة (SRP) |

---

## 2. التدقيق التفصيلي لمكونات الكومبايلر (ملفاً بملف)

### 2.1 الفئات الأحادية العملاقة (God Classes)

#### 📄 [`Statement.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Statement.cpp) & [`Statement.h`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Statement.h) (5,258 سطر)
* **المشاكل والأنماط القديمة المكتشفة:**
  1. **هيكل القائمة المرتبطة اليدوي:** استخدام `m_pNext` مع مؤشر خام وإلغاء متكرر للذاكرة عبر `delete pCurrent`.
  2. **تشتت المسؤوليات:** الملف يقوم بمسح الرموز (Lexing)، وتحليل الجمل (Parsing)، وفحص الأنواع (Type Checking)، وإصدار أوامر الـ Intermediate Instructions في نفس الفئة.
  3. **اعتماد كامل على `DWORD`:** معاملات الفحص والـ Token IDs ونطاقات الأسطر تُمرر كـ `DWORD`.
  4. **ارتباط بالمتغيرات العامة:** يستدعي مباشرة `g_pErrorReport`، `g_pVarTable`، `g_pDataTable`، `g_pLabelTable`، `g_pDBMWriter`.
* **الخطة الحديثة للترقية:**
  * تحويل تمثيل الجمل إلى عقد AST كاملة عبر `std::unique_ptr<ASTNode>`.
  * فصل محلل الجمل (Parser) عن بنية البيانات الممثلة للجملة (Statement AST Node).
  * استخدام نمط الزائر `ASTVisitor` لتوليد الكود أو التحقق الدلالي بدلاً من دوال التوليد المتشابكة.

---

#### 📄 [`ASMWriter.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/ASMWriter.cpp) & [`ASMWriter.h`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/ASMWriter.h) (2,997 سطر)
* **المشاكل والأنماط القديمة المكتشفة:**
  1. **مصفوفات التجميع الثابتة:** دالة `GenerateASMCodes()` تملأ مصفوفات ثابتة `m_iASMPreOp[300]`, `m_iASMOp1[300]`, `m_iASMOp2[300]` بأرقام سحرية (Magic Numbers) لأوامر x86 القديمة مع إضافة بادئات `0x48`.
  2. **الخلط بين 32-بت و 64-بت:** دوال مثل `WriteASMLine` تتوقع نصوصاً وتستخرج منها إزاحات كـ `DWORD`.
  3. **غياب بنية ABI رسمية:** غياب توليد جداول الـ Unwind Metadata الخاصة بـ Windows x64 SEH (`.pdata` / `.xdata`).
* **الخطة الحديثة للترقية:**
  * استبدال جدول المصفوفات الثابتة بفئة `X64CodeEmitter` أو `IRLoweringVisitor` حديثة ومهيكلة.
  * توليد تعليمات الآلة باستخدام واجهات صريحة الأنواع (`EmitMov(Reg64, Imm64)`, `EmitCall(Reg64)`).
  * إدماج دعم الـ Shadow Space وحساب محاذاة المكدس (16-byte Stack Alignment) بشكل رسمي عند كل نداء دالة.

---

#### 📄 [`InstructionTable.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/InstructionTable.cpp) (94,894 بايت) & [`MathOp.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/MathOp.cpp) (80,074 بايت)
* **المشاكل والأنماط القديمة المكتشفة:**
  1. **كتل ضخمة من `switch-case`:** تكرار يدوي لعمليات التحويل الحسابي والمنطقي لكل تركيبة من أنواع البيانات (Integer, Float, Double, String, Byte, Word).
  2. **تحويلات قسرية (C-Style Casts):** قص المؤشرات إلى أعداد صحيحة `(DWORD)` والعكس.
* **الخطة الحديثة للترقية:**
  * استخدام قوالب C++20 الحديثة (`Templates` و `Concepts` و `std::visit`) لمعالجة التوافق بين الأنواع.
  * تقليل حجم الكود المكرر بنسبة تتجاوز 70% عبر نمط التوليد المعمم (Generic IR Operator Lowering).

---

### 2.2 إدارة وتحليل النصوص والسلاسل (String Handling)

#### 📄 [`Str.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Str.cpp) & [`Str.h`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Str.h)
* **المشاكل والأنماط القديمة المكتشفة:**
  1. **تصميم يعود لعام 2001:** الفئة ترث من `db3::TObject<CStr>` وتعتمد على دوال معالجة مخصصة مثل `EatTrailingEdgeSpacesandTabs()`، `CropEqualEdgeBrackets()`، `EatSpeechMarks()`.
  2. **مؤشرات النصوص غير الآمنة:** تعيد وتستقبل `LPSTR` و `LPCSTR` و `char*` قابل للتعديل المباشر.
  3. **عنونة الأحجام بـ `DWORD`:** تستخدم `DWORD Length()` بدلاً من `std::size_t`.
* **الخطة الحديثة للترقية:**
  * التخلص التدريجي من `CStr` واستبدال وظائف القراءة فقط بـ `std::string_view` ووظائف البناء بـ `std::string`.
  * توفير دوال مساعدة معيارية لمعالجة النصوص داخل مساحة أسماء `dbp::string_utils` دون كائنات نصوص قديمة.

---

### 2.3 جداول الرموز والبيانات (Symbol & Data Tables)

#### 📄 [`VarTable.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/VarTable.cpp) / [`DataTable.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/DataTable.cpp) / [`StructTable.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/StructTable.cpp) / [`LabelTable.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/LabelTable.cpp)
* **المشاكل والأنماط القديمة المكتشفة:**
  1. **مصفوفات مؤشرات خام مخصصة يدوياً:** إدارة سعة الجداول عبر تخصيص كتل `new T[size]` ومضاعفة الحجم يدوياً مع استدعاء `memcpy`.
  2. **افتراض أن حجم المؤشر 4 بايت (32-بت):** حساب إزاحات الحقول والمتغيرات باستخدام مضاعفات 4 بايت القديمة في بعض مواضع الجداول القديمة.
  3. **غياب الأمان الخيطي (Not Thread-Safe):** اعتمادها على الحالة العامة ووجود مؤشرات عامة تشير إليها في كامل الكومبايلر.
* **الخطة الحديثة للترقية:**
  * ترحيل جداول الرموز إلى `std::vector<Symbol>` و `std::unordered_map<std::string, SymbolIndex, string_hash, std::equal_to<>>`.
  * استخدام `dbp::abi::TargetAbi64::address_size` لحساب الإزاحات وتوزيع الذاكرة ديناميكياً بدقة 8 بايت ومحاذاة 64-بت.

---

### 2.4 ملفات تحليل الجمل والتحكم (Parsing Modules)

#### 📄 [`ParseInstruction.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/ParseInstruction.cpp) / [`ParseUserFunction.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/ParseUserFunction.cpp) / [`ParseJump.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/ParseJump.cpp) / [`ParseLoop.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/ParseLoop.cpp)
* **المشاكل والأنماط القديمة المكتشفة:**
  1. **الاقتران الوثيق بالكلاس `CStatement`:** تمرير مؤشر `CStatement*` والتعديل على حالته الداخلية مباشرة.
  2. **المعالجة اليدوية لرموز الفواصل والأسطر:** استخدام أرقام التوكنز السحرية ومقارنتها بقيم `DWORD`.
* **الخطة الحديثة للترقية:**
  * إعادة صياغة وحدات التحليل كـ Pure Recursive Descent Parsers تُرجع `std::unique_ptr<ASTNode>`.
  * تمرير `TokenStream` منمط ومحصور بدلاً من القراءة المباشرة من مخازن الملفات المفتوحة.

---

### 2.5 إدارة الذاكرة وتوليد الملفات التنفيذية (EXE Generation & PE Loading)

#### 📄 [`EXEBlock.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/EXEBlock.cpp) & [`MemoryPE.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/MemoryPE.cpp)
* **المشاكل والأنماط القديمة المكتشفة:**
  1. **التعامل المباشر مع بايتات الـ PE Headers:** تعديل ترويسات `IMAGE_NT_HEADERS` بمؤشرات خام دون استخدام دوال مغلفة وآمنة.
  2. **تحويل العناوين (Pointer Arithmetic):** استخدام `(DWORD)ptr` في بعض مواضع حساب الإزاحات الموروثة من كود 32-بت.
* **الخطة الحديثة للترقية:**
  * نقل كافة عمليات بناء الـ PE إلى [`PEBuilder.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/PEBuilder.cpp) مع دعم كامل لـ `IMAGE_NT_HEADERS64`.
  * حظر أي تحويل مؤشر إلى `DWORD`، واستخدام `uintptr_t` / `size_t` حصراً.

---

### 2.6 واجهة الربط القديمة والإضافات (DLL Boundary & .rc Binding)

#### 📄 ربط الإضافات عبر ملفات `.rc` وجداول الأسماء المزخرفة (Decorated Names)
* **المشاكل والأنماط القديمة المكتشفة:**
  1. **هشاشة المانغلينغ (MSVC Name Mangling):** استدعاء دوال C++ الخارجية بالاعتماد على أسمائها المشوهة مثل `?LoadAnimation@@YAX_KH@Z` المخزنة داخل جداول نصوص `.rc`.
  2. **تهريب المؤشرات (Slot Pointer Smuggling):** تمرير المؤشرات في فتحات أرقام صحيحة واسترجاعها عبر `(char*)pFilename`.
* **الخطة الحديثة للترقية (كما في L2 Plan):**
  * بناء `CommandRegistry` صريح يسجل الدوال ومؤشراتها مباشرة دون ملفات موارد وسيطة.
  * اعتماد واجهة `extern "C"` مستقرة مع أنواع واضحة (`std::string_view`, `std::span`).

---

## 3. كتالوج الأنماط القديمة المطلوب استئصالها

```
┌─────────────────────────────────────────────────────────────────────────┐
│                      أنماط الليجاسي المطلوب إزالتها                      │
├────────────────────────────────┬────────────────────────────────────────┤
│ النمط القديم (Legacy Anti-Pattern)│ البديل الحديث المعياري (Modern C++20)  │
├────────────────────────────────┼────────────────────────────────────────┤
│ DWORD dwLength / DWORD dwIndex │ std::size_t / uint32_t / uint64_t      │
│ LPSTR / LPCSTR / char*         │ std::string_view / std::string         │
│ extern CVarTable* g_pVarTable  │ Context Dependency Injection           │
│ CStr (custom 2001 string class)│ std::string / std::string_view         │
│ new T[] + memcpy + delete[]    │ std::vector<T>                         │
│ Linked lists (m_pNext + delete)│ std::vector<std::unique_ptr<Node>>     │
│ Decorated Names in .rc files   │ Typed CommandRegistry / extern "C"     │
│ Pointer casts to (DWORD)       │ std::uintptr_t / dbp::abi::TargetAbi64 │
│ Monolithic 5000+ line files    │ Modular Decoupled Single-Responsibility│
│ Magic error integer return     │ std::expected<T, DiagnosticError>      │
└────────────────────────────────┴────────────────────────────────────────┘
```

---

## 4. مصفوفة ملفات الكومبايلر والحالة الفنية لكل منها

| اسم الملف | الحجم (أسطر/بايت) | الأنماط القديمة الحالية | درجة التعقيد | أولوية التحديث |
|---|---|---|---|---|
| [`Statement.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Statement.cpp) | 5,258 سطر | God Class، مؤشرات خام، قوائم يدوية، كثرة `DWORD` | حرجة جداً | 🔴 المرحلة 3 |
| [`ASMWriter.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/ASMWriter.cpp) | 2,997 سطر | جداول أوبكود ثابتة، خلط سجلات، غياب SEH Unwind | حرجة جداً | 🔴 المرحلة 4 |
| [`InstructionTable.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/InstructionTable.cpp) | 94.8 KB | تحويلات قسرية، سويتشات ضخمة، تكرار الأكواد | عالية | 🟠 المرحلة 3 |
| [`MathOp.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/MathOp.cpp) | 80.0 KB | منطق حسابي مكرر لكل نوع بدائي | متوسطة | 🟠 المرحلة 4 |
| [`Str.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Str.cpp) / [`Str.h`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/Str.h) | 32.3 KB | فئة نصوص موروثة عام 2001، `LPSTR`، إزاحات `DWORD` | عالية | 🔴 المرحلة 2 |
| [`VarTable.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/VarTable.cpp) / [`DataTable.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/DataTable.cpp) | 34.4 KB | مصفوفات يدوية `new[]`، حالة عامة `extern` | متوسطة | 🟡 المرحلة 5 |
| [`StructTable.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/StructTable.cpp) | 13.5 KB | حساب محاذاة يدوي، مؤشرات خام | متوسطة | 🟡 المرحلة 5 |
| `Parse*.cpp` (7 ملفات) | ~60 KB | اقتران وثيق بـ `CStatement`، أرقام توكنز سحرية | متوسطة | 🟠 المرحلة 3 |
| [`EXEBlock.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/EXEBlock.cpp) | 67.3 KB | إدارة كتل الذاكرة التنفيذية وحساب الإزاحات | متوسطة | 🟡 المرحلة 5 |
| [`PEBuilder.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/PEBuilder.cpp) | 15.6 KB | *(تم تحديثه جزئياً لدعم PE32+ 64-bit بنجاح)* | منخفضة | 🟢 محدث |
| [`MachineCodeBuffer.cpp`](file:///d:/GitHub-repo/FPC%20Creator%20Repos/Dark-Basic-Pro/DBProCompiler/DBPCompiler/MachineCodeBuffer.cpp) | 3.5 KB | *(تم استخراجه وتحديثه كفئة مستقلة آمنة الحدود)* | منخفضة | 🟢 محدث |
