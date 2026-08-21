# خطة التحديث المعماري (L2) — استبدال آلية `.rc` + فتحات `DWORD`

> وثيقة تصميم قابلة للتنفيذ لاحقاً. الهدف: إحلال طبقة الربط بين مفسّر BASIC ودوال الأوامر
> بآلية حديثة آمنة الأنواع، بدل جداول الأسماء المزخرفة (`Decorated Names`) وفتحات `DWORD` التي تُهرب المؤشرات.

---

## 1. الخلفية (لماذا هذا ضروري؟)

البنية الحالية (إرث من مطلع الألفية):

```
كود BASIC للمستخدم
   │  (اسم الأمر المقروء، مثل "LOAD ANIMATION")
   ▼
مفسّر DBPro
   │  يبحث في جدول STRINGTABLE بملف .rc عن الاسم المزخرف:
   │     "LOAD ANIMATION%SL%?LoadAnimation@@YAX_KH@Z%..."
   ▼
GetProcAddress( hDLL, "?LoadAnimation@@YAX_KH@Z" )
   │
   ▼
دالة C++: LoadAnimation( DWORD_PTR pFilename, int animindex )
              │  تفسّر pFilename كـ (char*) → النص
```

المعاملات تُمرّر في **فتحات ثابتة** (slots)، والدالة تُعيد تفسير كل فتحة حسب نوعها
(نص ← مؤشر مهرب داخل `DWORD`/`DWORD_PTR`، رقم ← `int`، …).

**مشاكل هذه البنية:**
- **هشاشة**: أي تغيير بالتوقيع دون تحديث `.rc` يكسر الربط **بصمت** (لا خطأ ترجمة ولا رسالة تشغيل).
- **عدم المحمولة**: المانغلينغ خاص بـ MSVC؛ يختلف بين إصدارات Visual Studio ومع GCC/Clang.
- **خطر 64 بت**: تهريب المؤشرات في فتحات 32/64 بت سبّب الحاجة لتغيير `DWORD`→`DWORD_PTR`.
- **إعاقة التحديث**: لا يمكن تمرير `std::string_view`/`std::span` أو أنواع قوية.

> ملاحظة: تغيير `DWORD`→`DWORD_PTR` (المرحلة 0) يُبقي هذه البنية حيّة على 64 بت،
> لكنه **لا يلغيها**. هذه الوثيقة هي البديل الجذري.

---

## 2. الهدف (Goal)

استبدال طبقة الربط بآلية:

1. **تسجيل صريح وآمن الأنواع** — لا أسماء مزخرفة في ملفات نصية.
2. **أنواع حقيقية عند الحدود** — `std::string_view` / `std::span` / أنواع قوية، بدل تهريب مؤشرات.
3. **حدود DLL مستقرة** — لا اعتماد على مانغلينغ C++ عبر المكتبات الخارجية.

---

## 3. غير ضمن الهدف (Non-Goals)

- **لا** توافق مع ملفات الأصول القديمة (`.dbo` / `.x` / `.fpo`) — قرار سابق: قطع نظيف.
- **لا** تغيير لغة BASIC نفسها ولا سلوك الأوامر الظاهر للمستخدم (أسماء الأوامر تبقى كما هي).

---

## 4. البنية الهدف (Target Architecture)

```
كود BASIC
   │
   ▼
مفسّر DBPro
   │  يبحث في "سجل الأوامر" (Command Registry) عن الاسم المقروء
   │  ← السجل يربط الاسم مباشرةً بمؤشر دالة مُنمَّط (std::function / fn ptr)
   ▼
دالة الأمر (بنوعها الحقيقي):
   LoadAnimation( std::string_view filename, int animindex )
   │  لا reinterpret_cast، لا فتحات، لا أسماء مزخرفة
```

طبقة **تحويل واحدة** (Value Adaptor) تترجم قيم المفسّر الديناميكية إلى معاملات مُنمَّطة
في نقطة واحدة مفحوصة — بدل توزّع التحويلات على كل دالة.

---

## 5. خطوات التنفيذ (مراحل)

### المرحلة 0 — استقرار 64 بت (قيد التنفيذ الآن)
- [x] سحبة `DWORD`→`DWORD_PTR` للدوال التي تحمل مؤشرات (أنجزت: `LoadAnimation`, أوامر الكاميرا, `dbWriteStringToRegistry`, `dbDLLLoad`, `dbLimbName`, `LMSetLightMapName/Folder`, `DBOConvertBlockToObject/DBOSaveBlockFile`, `ReverseString`).
- [x] مزامنة `.rc` الموافق: `Animation.rc:116` (`?LoadAnimation@@YAXKH@Z` → `?LoadAnimation@@YAX_KH@Z`).
- [ ] **مؤجّل**: `dbCallDLLX`/`CallDLLX` (أمر `CALL DLL X`) — اسمه المزخرف في `.rc` غير اعتيادي (`?CallDLLX@@YA_KH_K0K@Z`)؛ يتطلب بناءً لاستخراج الاسم الصحيح قبل التغيير. **مرشّح ليكون أول ما يُستبدل بالسجل.**
- [ ] **متابعة**: `DBOLoadBlockFile` يخزّن المؤشر في `DWORD*` عبر `(DWORD)new char[]` (يُقطع على 64 بت) — خلل أعمق يُصلح في المرحلة 2/3.
- [ ] بناء أخضر بـ `/permissive-` + `cxx_std_20` + فحص مزامنة `.rc`/`.cpp` يدوي.

### المرحلة 1 — سجل أوامر مُنمَّط (Typed Command Registry)
- تعريف نوع الأمر:
  ```cpp
  using CommandFn = std::function<void(CommandArgs const&)>;
  struct Command { std::string name; CommandFn handler; /* بيانات نوع المعاملات */ };
  ```
- حاوية مركزية:
  ```cpp
  class CommandRegistry {
  public:
      void register_command(std::string_view name, CommandFn fn);
      Command* find(std::string_view name);
  private:
      std::unordered_map<std::string, Command, string_hash, std::equal_to<>> commands_;
  };
  ```
- استبدال قراءة `.rc` بنداءات تسجيل صريحة عند تحميل كل DLL/مكتبة أوامر.
- **إبقاء أسماء الأوامر المقروءة كما هي** (لغة المستخدم لا تتغير).

### المرحلة 2 — أنواع حقيقية عند الحدود + طبقة التكيّف
- تغيير تواقيع معالجات الأوامر:
  ```cpp
  // قبل:  void LoadAnimation( DWORD_PTR pFilename, int animindex )
  // بعد:
  void LoadAnimation( std::string_view filename, int animindex );
  ```
- طبقة `Value Adaptor` واحدة تترجم قيمة المفسّر → النوع المطلوب:
  ```cpp
  template<typename T> T convert(InterpValue const& v);
  // تخصصات: string_view ← سلسلة المفسّر، int/float ← أعداد، span ← كتلة/مصفوفة
  ```
- حل `DBOLoadBlockFile`: تخزين الكتلة في `std::vector<std::byte>` أو `void*`/`DWORD_PTR*` بدل `DWORD*`.

### المرحلة 3 — حدود DLL مستقرة (للإضافات)
- تعريف واجهة DLL بـ `extern "C"` نظيفة أو واجهة وهمية بأسلوب COM:
  ```cpp
  // C-ABI مستقر، مُصدَّر بأرقام إصدار
  extern "C" PLUGIN_API IPlugin* CreatePlugin(uint32_t abi_version);
  ```
- **التوقف عن ربط أسماء C++ المزخرفة عبر DLL** (مانغلينغ خاص بالمترجم).
- المعرّفات (handles) تُمرّر كـ `void*` / `std::uintptr_t` مع واجهة مُصدَّرة مُرقّمة الإصدار.

### المرحلة 4 — أتمتة، اختبار، وCI
- اختبارات `gtest` تغطي سجل الأوامر (كل أمر مسجّل وقابل للاستدعاء).
- سكربت/Linter يكشف أي انحراف بين التعريف والاستدعاء (بديل آلي لفحص `.rc`).
- أنبوب تكامل مستمر (CI) يبني ويشغّل الاختبارات على مصفوفة إعدادات (x64 / `/permissive-`).

### المرحلة 5 (اختيارية) — Modules وانعكاس ثابت
- ترحيل الترويسات إلى **C++20 Modules** (بناء أسرع وعزل أفضل).
- عند توفّر **الانعكاس الثابت (C++26)**: توليد جداول التسجيل تلقائياً من تعريفات الأوامر،
  فيُلغى كتابتها يدوياً وكذلك الحاجة إلى `.rc`.

---

## 6. رسم الخرائط إلى ميزات C++ الحديثة

| المشكلة القديمة | البديل الحديث |
|---|---|
| `DWORD pString` (مؤشر مهرب) | `std::string_view` / `const char*` |
| `DWORD pBlock` (مؤشر مهرب) | `std::span<std::byte>` / `void*` |
| جدول `.rc` نصّي | `CommandRegistry` صريح |
| `GetProcAddress` باسم مزخرف | مؤشر دالة مُسجَّل / `extern "C"` ABI |
| تحويلات موزّعة (`(char*)`) | `Value Adaptor` مركزي واحد |
| ترويسات نصّية هشّة | C++20 Modules |
| كتابة يدوية للمسجل | انعكاس ثابت (C++26) |

---

## 7. المخاطر والتراجع (Risks & Rollback)

- **حجم التغيير**: المرحلتان 1–2 كبيرتان. يُنصح بشيم (shim) مؤقت يربط السجل القديم
  أثناء الانتقال تدريجياً دون كسر الأوامر.
- **أسماء الأوامر**: يجب أن تبقى كما هي تماماً وإلا انكسر كود المستخدمين (ليس الأصول، بل الأكواد).
- **DLL الإضافات الحالية**: أي إضافة تعتمد على المانغلينغ ستتوقف — تتطلب تحديثها للواجهة الجديدة (المرحلة 3).
- **التراجع**: كل مرحلة مُرقّمة-versioned؛ يمكن إيقاف ما قبلها عبر أعلام (feature flags).

---

## 8. ملاحظات من العمل الحالي (Grounding)

استُخرجت هذه المواقع أثناء تدقيق المستودع وهي الأساس العملي للخطة:

- `Dark Basic Pro SDK/Shared/Animation/CAnimation.cpp:1253` + `DarkSDK/Animation/Animation.rc:116`
  (مثال على انفصال `.rc`/`.cpp` الذي تحلّه المرحلة 1).
- `Shared/System/CSystemC.cpp:1049` `dbCallDLLX` / أمر `CALL DLL X` (`System.rc:160`) —
  مرشّح أول للاستبدال بالسجل (مانغلينغ غير اعتيادي).
- `Official Plugins/DarkLIGHTS/LightMapper/DBOBlock.cpp` + `Shared/DBOFormat/DBOBlock.cpp`
  (نسخ مكرّرة — توضّح لماذا التسجيل المركزي أفضل من التوزّع).
- `DBOLoadBlockFile` يخزّن مؤشراً في `DWORD*` ← خلل 64 بت يُصلح في المرحلة 2/3.

---

## 9. أسئلة مفتوحة (Open Questions)

1. هل نحتفظ بدعم `.rc` القديم عبر طبقة توافق مؤقتة، أم قطع كامل فوراً؟
2. متى ننتقل إلى C++20 Modules (بعد استقرار السجل أم بالتوازي)؟
3. هل نعتمد `std::string_view` فقط، أم نضيف أنواعاً قوية (strong types) لكل معامل أمر؟
