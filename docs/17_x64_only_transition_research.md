# بحث: التحول الكامل إلى x64 فقط (Full x64-Only Transition)

> وثيقة بحثية مستندة إلى فحص فعلي للمستودع (فرع `test-x64-full`، أغسطس 2026).
> الغرض: تقييم جدوى إسقاط دعم x86 (32-bit) نهائيًا وجعل x64 هو الهدف الوحيد،
> مع تحديد العوائق الحاسمة وخارطة الطريق الواقعية.

---

## 1. الخلاصة التنفيذية

**التحول الكامل إلى x64 فقط قابل للتنفيذ اليوم**، والمتبقي عمل هندسي لا عوائق خارجية.
**التصحيح الجوهري (أغسطس 2026)**: أكبر محرك رسومات ثلاثية الأبعاد (`Basic3D` /
`DBProBasic3DDebug.dll`) **له مصدر كامل في هذا المستودع** — هو مشروع `DarkSDK/Objects`
الذي يُنتج الـ DLL (تكافؤ تصديرات 708/709 مع الثنائي المشحون)، وسلالة GameGuru MAX
بنته x64 فعلًا (`Basic3D.lib` — تحقّق `8664 machine (x64)`). التفاصيل والأدلة في
`docs/19_x64_basic3d_source_findings.md`. المتبقي فعليًا: مجموعة DLLs الرسمية في
`Install/Compiler` كلها PE32/x86 وتحتاج بناء x64 من مصادرها.

ما هو ممكن ويُبنى عليه فعليًا الآن:

| المستوى | الحالة اليوم |
| :--- | :--- |
| مضيف المترجم (DBPCompiler.exe) بـ x64 | ✅ ممكن (Presets موجودة) |
| إخراج برامج PE32/x86 | ✅ يعمل ويُختبر (conformance) |
| فصْل ABI الهدف عن المضيف (`TargetAbi64`) | ✅ منجز ومُختبَر |
| صانع الأكواد x64 (`CASMWriterx64`) | 🟡 مبكّر (جزء صغير من الأوبكود) |
| توليد PE32+ كامل | 🟡 تحقق جزئي (تحقق صحة فقط في `CPEBuilder`) |
| وقت تشغيلي 64-bit (DLLs) | 🟡 المصدر متاح (محرك 3D = مشروع `Objects`، وتكافؤ تصديراته 708/709)؛ البناء x64 مثبت من سلالة GameGuru MAX — المتبقي جرد مؤشرات + بناء 33 DLL |
| مشغّل/مصحّح 64-bit | ❌ غير مكتمل (نقل مؤشرات DWORD وغيرها) |

**الخلاصة**: الخيار الواقعي هو **"x64 أولًا مع إبقاء x86 كاحتياط"** حتى يكتمل
الوقت التشغيلي، ثم إسقاط x86 كخطوة أخيرة بعد معايير جاهزية واضحة (انظر §5).

---

## 1.5 حالة التنفيذ (تحديث: أغسطس 2026)

نُفِّذ فعليًا في هذه الجولة:

| العمل | الحالة | التفاصيل |
| :--- | :--- | :--- |
| إصلاح خلل المقابض 32-bit | ✅ | استبدال `(HANDLE)0xFFFFFFFF` بـ `INVALID_HANDLE_VALUE` في 9 مواضع (Error.cpp، DBPCompiler.cpp، DarkEXE.cpp، DBDLLCore.cpp، CFileC.cpp) + حُرّاس NULL — هذا كان سبب انهيار 73 اختبارًا على x64 |
| اختبارات x64 على المترجم | ✅ | من 73 فشلًا إلى **0 فشل** (882 اختبارًا ناجحًا) على `windows-x64-debug` |
| تحويل مؤشرات الاختبارات | ✅ | اختبارات ArrayIndex/StackQueue انتقلت من `DWORD` إلى `DWORD_PTR` (قصّ مؤشر) |
| محمّل PE في الذاكرة | ✅ | `MemoryPE` أصبح محمّلًا مزدوج PE32/PE32+ يرفض PE32 نظيفًا على مضيف x64، مع عيّنة اختبار PE32+ مبنية من المصدر |
| حذف `CASMWriterx64` | ✅ | حُذف الملفان واندمجت تعليمات x64 (REX، XMM، ABI المساعدات) في `CASMWriter` نفسه — لا يوجد كاتب 64 منفصل |
| نظام البناء x64 فقط | ✅ | `CMakeLists.txt` يجعل x64 الافتراضي ويرفض أي تكوين 32-bit بخطأ قاطع؛ `CMakePresets.json` كله x64 (مع asan/ubsan/coverage/clang-tidy)؛ CI (`windows-x64.yml`) كله x64 |
| السكربتات والوثائق | ✅ | `run-local-ci.ps1` و `coverage-report.ps1` و `README.md` حُدِّثت إلى presets x64 |

**المرحلة التالية (الموجة 2):** تحويل إصدار الكود الفعلي في `CASMWriter` من x86 إلى x64 —
إعادة كتابة الأوبكود (REX/عناوين 8 بايت/RIP-relative)، تحويل `PEBuilder` لإخراج PE32+ كامل،
وتحويل وقت التشغيل (DBDLLCore وغيرها من المصادر) من `DWORD` إلى `DWORD_PTR` في دوال الصفيف،
ثم بناء DLLs التشغيلية كـ x64.

---

## 2. الوضع الحالي في المستودع (مستند إلى الفحص)

### 2.1 نظام البناء
- `CMakeLists.txt` الافتراضي هو `Win32`، مع `windows-x64-debug/release` في
  `CMakePresets.json` — لكن **CI الوحيد** (`.github/workflows/windows-x86.yml`)
  يبني 6 إعدادات x86 فقط ولا يختبر x64 إطلاقًا.
- المصدر يُجمَّع نظيفًا في وضعي x86 و x64 للمكوّنات الرئيسية (المترجم والمشغل
  والاختبارات)، وخطة `2026-08-10-x64-presets-ci-matrix.md` تعالج إضافة x64 إلى CI.

### 2.2 سلسلة تنفيذ البرنامج المولَّد (منظومة كاملة يجب أن تصبح 64-bit)
برنامج DarkBASIC المولَّد ليس ثنائيًا قائمًا بذاته؛ هو **حزمة ذاتية الفك**
(Self-extracting): قشرة `DarkEXE` + حمولة مضمّنة. عند التشغيل:
1. `DarkEXE` (WinMain) يفك الحزمة ويوثّقها (VFS / SHA-256).
2. `CEXEBlock::Load/Run` يحمّل **كتلة الكود الآلي المولَّدة من الكومبايلر**
   (حاليًا x86 من `CASMWriter`) ويُنفذها بعد `VirtualProtect(PAGE_EXECUTE_READ)`.
3. يحمّل DLLs وقت التشغيل عبر `LoadLibrary`/`GetProcAddress`
   (`DBProCore.dll`, `DBProBasic3DDebug.dll`, ...) ويستدعي `Constructor` لكل منها.

لذلك "x64 فقط" تعني تحويل **كل** هذه الطبقات، وليس المترجم وحده.

### 2.3 صانع الأكواد
- `ASMWriter.cpp` (x86): **2,465 سطرًا** — هو المُستخدَم فعليًا لإخراج البرامج.
- `ASMWriterx64.cpp`: **183 سطرًا** — أساس ABI (تسجيل المعاملات RCX/RDX/R8/R9،
  32 بايت shadow space، محاذاة 16) + مجموعة أوبكود صغيرة (MOV/PUSH/POP/ADD/SUB/RET/
  CMP/TEST/JMP/JNE/JE/CALL/NOP + MOVSS/ADDSS/MULSS) — ما زال بعيدًا عن التكافؤ
  الكامل مع ما تحتاجه الواجهة الأمامية من تعليمات.
- البنية مهيأة جيدًا: `ICodeGenerator` + `CodeGenerationSession` + فكّ عناوين
  عبر `TargetABI` بعرض مستقل عن المضيف، وعناوين مرجعية رمزية حتى حدود التسلسل
  (pointer-width-independent references).

### 2.4 توليد الصورة PE
- `PEBuilder` (501 سطرًا) يتضمن **تحقق صحة** لمواصفات PE32+ (محاذاة، صورة
  قاعدية، Machine=AMD64) — وهو الأساس الجاهز — لكنه لا يُنتج صورة PE32+ كاملة
  بعد (لا يوجد توليد `.pdata/.xdata` للإزاحة/الاستثناءات، ولا توليد جدول
  استيراد 64-bit كامل في المسار المستخدم).
- يوجد أيضًا مسار تحميل في الذاكرة (`MemoryPE`) يجب أن يعالج صور 64-bit.

### 2.5 وقت التشغيل والـ Plugins (أهم عائق)
- كل DLLs وقت التشغيل في `Install/Compiler/plugins/` و `plugins-licensed/`
  و `plugins-user/` هي **PE32/i386** (تحقّق فعلي بـ `file`):
  `DBProCore.dll`, `DBProBasic3DDebug.dll`, `DBProParticlesDebug.dll`,
  `Conv3DS.dll`, `DBProGameFX.dll`, `DBProODEDebug.dll`, ...
- لدى **~28 plugin** مصادر في
  `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/`، وقد أُدمجت في مصفوفة
  بناء CMake كمكتبات **STATIC** تدعم x86/x64 (لأغراض الاختبار والتحقق).
  لكن **الوقت التشغيلي الفعلي للبرامج المولَّدة هو ملفات DLL الثنائية**،
  فإعادة بناء المصادر كـ x64 خطوة ضرورية لكنها غير كافية وحدها.
- **محرك 3D (`Basic3D` / `DBProBasic3DDebug.dll`) له مصدر كامل** — هو مشروع
  `DarkSDK/Objects` (يُنتج الـ DLL؛ سجل أوامره في `Objects.rc` بنفس الأسماء المزخرفة
  التي تحلّها المكونات من `g_Basic3D`). كثير من المكونات تعتمد عليه عبر
  `GetProcAddress`: `Core/DBDLLExtCalls`, `Camera`, `Setup/CGfxC`, `Q2BSP/Q3BSP`,
  `ODE`, `GameFX`, `Image` — لكنه قابل للبناء x64 من المصدر (تكافؤ تصديرات
  708/709 مع الثنائي المشحون؛ وسلالة GameGuru MAX بنته x64 فعلًا — راجع
  `docs/19_x64_basic3d_source_findings.md`).
- `Multiplayer`/`MultiplayerPlus`: المصدر موجود لكنه مستثنى من CMake
  (اعتماد DirectPlay قديم بلا رؤوس حديثة).
- Plugins رقمية مغلقة إضافية: `DarkPHYSICS (ODE)`, `DarkLIGHTS`,
  `GameFX (licensed)`, المحوِّلات (Conv3DS/MD2/MD3/MDL/X) — بعضها له مصدر
  في المجلدات لكن النسخ الموزَّعة ثنائية فقط.

### 2.6 عرض المؤشرات (Pointer Width)
- طبقة `TargetAbi32/64` عزلت عرض عناوين **المترجم** بنجاح (قراءات/كتابات آمنة
  بحدود) — وهذا ما يمنع مضيف x64 من تغيير تنسيق البرامج 32-bit صامتًا.
- لكن الجانب **التشغيلي** مليء بافتراضات 32-bit:
  - `DarkEXE.cpp`: إيداع `g_pGlob` في ملف-مشترك للمصحح كـ `(DWORD)` — قصّ مؤشر.
  - `Text/CTextC.cpp`: تمرير سلاسل كمعاملات `DWORD` (`BasicText(DWORD szText)`).
  - أنماط `SetWindowLong(..., (LONG)...)` وغيرها في المكونات.
  - تنسيق إعادة توزيع الكود المولَّد وعناوينه الداخلية (4 بايت) — يحتاج
    توسيعًا إلى 8 بايت مع معالجة RIP-relative.
- جرد شامل عبر مكونات ~40 plugin مطلوب، وقد بدأت خطط لهذا في مسار
  `2026-08-10-x64-plugin-*`.

### 2.7 المصحّح
- `DBPDebugger` و `DebuggerInterface` يتواصلان عبر shared-memory filemaps؛
  نقل المؤشرات بعرض 4 بايت يجب أن يصبح 8 بايت (بتنسيق متوافق مع إصدار).

---

## 3. ماذا يعني "x64 فقط" عمليًا؟ (ثلاثة تفسيرات)

| التفسير | المعنى | الجدوى |
| :--- | :--- | :--- |
| **أ. مضيف x64 فقط** | الأدوات (المترجم/المصحح/المشغل) تُبنى x64، لكن البرامج المولَّدة تبقى PE32/x86 | ✅ قريب التحقيق (البنية تفصل الهدف عن المضيف)؛ يستفيد المترجم من ذاكرة >4GB |
| **ب. إخراج x64 فقط** | كل برنامج مولَّد يصبح PE32+/x64 | 🟡 ممكن بعد اكتمال §2.2–§2.6 وبناء DLLs x64 من المصادر (محرك 3D مصدره متاح) |
| **ج. إسقاط x86 نهائيًا** | حذف مسارات Win32 من البناء و CI والمصدر، وعدم القدرة على إخراج x86 إطلاقًا | ❌ غير ممكن قبل حل العوائق في ب |

السؤال الذي يُطرح غالبًا "التحول الكامل إلى x64 فقط" = **ج**، وأساسه **ب**.

---

## 4. العوائق الحاسمة (Blockers)

1. **بناء مجموعة DLLs x64 من المصادر** — المصدر متاح (محرك 3D = مشروع `Objects`،
   وتكافؤ تصديراته 708/709 مع الثنائي المشحون؛ وسلالة GameGuru MAX بنته x64 فعلًا).
   العمل المطلوب: تحويل مشاريع `DarkSDK/*` إلى x64 (Core + Objects أولًا) مع اختبار
   تكافؤ تصديرات تلقائي مقابل الثنائيات المشحونة، وتوفير **DirectX SDK القديم
   (2007)** الذي يعتمد عليه المحرك في البناء.
2. **مكتملية `CASMWriterx64`** — التكافؤ الكامل مع ما تستدعيه الواجهة
   الأمامية من أوبكود، والتصحيح النسبي 32-bit للقفزات والبيانات، وعناوين
   8 بايت، واستدعاءات DLL بـ x64 calling convention مع shadow space.
3. **PE32+ كامل + معلومات الإزاحة (unwind)** — توليد `.pdata/.xdata`
   لاستثناءات النظام؛ معمارية JIT تضع قيودًا على استثناءات C++ SEH عبر
   إطارات كود مولَّد — يحتاج تصميمًا صريحًا (تشغيل كود JIT خلف حدود
   `SetUnhandledExceptionFilter` بدل `__try/__except` الداخلية مثلاً).
4. **جرد عرض المؤشرات في الوقت التشغيلي** — مكونات ~40 plugin بأكواد من
   2004 تحمل افتراضات DWORD/4-byte.
5. **Plugins مغلقة إضافية** — ODE/DarkPHYSICS، DarkLIGHTS، GameFX، المحوِّلات:
   كل منها يحتاج مصدرًا أو بديلًا مفتوحًا (المشروع نفسه يملك Bullet و ODE
   كمصادر في `SDK/` — يمكن البناء منها).
6. **النظام البيئي للمستخدمين** — إعادة الترجمة تُنتج x64؛ منصّات
   (plugins) طرف ثالث مبنية على ABI غير مصنّف النسخة (globstruct) ستحتاج
   إعادة بناء.

---

## 5. خارطة طريق مقترحة (مراحل مع بوابات قرار)

### المرحلة 1 — إكمال المسار x64 في المترجم (لا تغيير على المخرجات)
- إكمال تكافؤ أوبكود `CASMWriterx64` + توسيع الاختبارات
  (`test_x64_assembler`, `test_x64_intensive_verification`, E2E).
- كتاب PE32+ كامل في `CPEBuilder` (رأس، أقسام، جدول استيراد، `.pdata`).
- إضافة إعدادات x64 إلى CI (`2026-08-10-x64-presets-ci-matrix.md`) مع
  الإبقاء على x86.

### المرحلة 2 — وقت تشغيلي x64 من المصادر المفتوحة
- بناء مجموعة الـ plugins المفتوحة المصدر (Core + ~28 plugin) كـ **DLLs x64**
  واستبدال النسخ الثنائية في `Install/Compiler` مؤقتًا في بيئة اختبار.
- تشغيل حزمة conformance كاملة على مخرجات x64 وإصلاح أخطاء عرض المؤشرات
  المكتشفة (خطة `2026-08-10-x64-plugin-core-globstruct` بدأت هذا).
- توسيع تنسيق المصحح والمشغل إلى 8 بايت.

### المرحلة 3 — بناء DLLs x64 من المصادر (محرك 3D مصدره متاح)
- بناء `DBDLLCore` و`Objects` (المحرك) كـ DLLs x64 أولًا مع اختبار تكافؤ تصديرات
  تلقائي مقابل الثنائيات المشحونة (المنهجية في `docs/19_x64_basic3d_source_findings.md` §2.2).
- ثم باقي البلوقنات المفتوحة (~28) واستبدال النسخ الثنائية في `Install/Compiler`
  مؤقتًا في بيئة اختبار.
- استبدال أو إعادة بناء ODE/Bullet/GameFX/المحوِّلات من مصادرها عند توفّرها.
- **بوابة القرار**: لا يُجعل x64 المخرج الافتراضي إلا بعد اجتياز conformance
  على x64 ووجود نسخ x64 لكل DLL يعتمد عليه أي برنامج يُختبر.

### المرحلة 4 — الإسقاط النهائي لـ x86 (عند استيفاء الشروط)
- معايير الجاهزية المقترحة:
  1. حزمة conformance (كل الحالات) تمر على مخرجات x64.
  2. كل DLL في `Install/Compiler/{plugins,plugins-licensed,plugins-user}`
     متوفر بنسخة x64 من مصدر مرخّص.
  3. قرار معلن بخصوص plugins الطرف الثالث.
- بعدها: حذف `windows-x86-*` من Presets و CI، إزالة `/safeseh`، تبسيط
  `CMAKE_GENERATOR_PLATFORM` إلى x64 فقط، وحذف فروع PE32 الميتة.

---

## 6. المخاطر والتخفيف

| الخطر | التخفيف |
| :--- | :--- |
| تراجع في سلوك البرامج 32-bit الحالية | إبقاء golden tests على x86 كمرجع أثناء نضوج x64 |
| كود JIT أكبر من 2GB (قفزات rel32) | اعتماد RIP-relative والعناوين 8 بايت من البداية في CASMWriterx64 |
| انهيار استثناءات على إطارات JIT | تصميم صريح لمعالجة الأخطاء خارج `__try` (مرجع: إصلاحات SEH في `DarkEXE.cpp` — `TryRunProgram`) |
| انقسام النظام البيئي (مستخدمون بصيغتين) | فترة انتقالية "x64 افتراضيًا مع خيار x86" وبيان توافق واضح |
| انحراف سلالة المصدر عن الثنائي المشحون (توقيع 2015) | اختبار تكافؤ تصديرات تلقائي (782 دالة) عند كل بناء x64 للمحرك |

---

## 7. توصية

1. **الاستمرار بمسار "x64 أولًا"** — المراحل 1–2 قابلة للتنفيذ الآن ولا تحمل
   خطرًا، وتُنتج مكاسب فورية (مضيف x64، اختبارات x64 في CI).
2. **عدم إسقاط x86 قبل المرحلة 3** — المتبقي هو **وقت التشغيل**: بناء 33 DLL كـ x64
   من مصادرها (محرك 3D مصدره متاح — راجع `docs/19_x64_basic3d_source_findings.md`)
   وجرد عرض المؤشرات؛ قرار "x64 فقط" الكامل معلَّق على اكتمال ذلك، لا على أي
   عائق خارجي.
3. **جعل "عدم وجود نسخة x64 لأي DLL يُحمَّل" خطأً وقت الترجمة** — فحص في
   `CEXEBlock`/المترجم يرفض ربط DLL 32-bit عند بناء هدف x64، فيتحول أي
   نقص مستقبلي إلى خطأ واضح مبكر بدل فشل وقت التشغيل.

---

## مراجع داخل المستودع

- `README.md` — الوضع الرسمي: PE32 نشط وPE32+ هدف مستقبلي.
- `docs/15_x64_and_future_tech.md` — الرؤية الأصلية للـ x64 (JIT + DLLs + جرد المؤشرات).
- `docs/superpowers/specs/2026-08-01-target-abi-foundation-design.md` — فصل ABI الهدف عن المضيف.
- خطط `docs/superpowers/plans/2026-08-10-x64-*` — الجدول الزمني الحالي للـ x64.
- `DBProCompiler/DBPCompiler/{ASMWriter.cpp, ASMWriterx64.cpp, PEBuilder.cpp, TargetABI.h}`
- `DBProCompiler/DBPCompilerEXE/DarkEXE.cpp` — نموذج التشغيل (قشرة + حمولة + DLLs).
- `Install/Compiler/plugins*/` — ملفات DLL وقت التشغيل (PE32/i386 حاليًا).

---

## 8. جرد شامل لبقايا 32-bit (فحص كامل للمستودع — أغسطس 2026)

> منهجية: فحص فعلي للمستودع (grep/تصنيف برمجي لجدول الأوبكود + عدّ القوالب
> النمطية `(DWORD)(ptr)` + فحص `file` للثنائيات المثبّتة). النسب أدناه على
> مستوى **الوظيفة** (كل منطقة تُقدَّر من 0 إلى 100%) وليس سطورًا، لأن حجم
> السطور لا يعكس الأثر التشغيلي.

### 8.1 خلاصة رقمية (نسبة "ما زال 32")

| المنطقة | الحالة | لا يزال 32 |
| :--- | :--- | :--- |
| نظام البناء + المضيف (CMake/presets/CI) | ✅ x64 حصري مع حارس رفض 32-bit | 0% |
| محمّل MemoryPE (PE32/PE32+) | ✅ مزدوج العرض، يرفض 32-bit على مضيف x64 | 0% |
| إصلاحات `INVALID_HANDLE_VALUE` (9 مواضع) | ✅ | 0% |
| أدوات إصدار x64 (REX/RIP-relative/SSE) | ✅ جزء من CASMWriter (مدمجة ومختبَرة) | 0% |
| جدول الأوبكود `GenerateASMCodes` (بايتات) | 🟡 179/210 صالحة كبايتات x64 | 15% (31 تعليمة) |
| دلالات الأوبكود + مواقع الاستدعاء (Statement/MathOp/TaskEmitter) | 🔴 مسار التشغيل الفعلي ما زال 32-bit | ~85% |
| آلية الاستدعاء (Calling Convention) | 🔴 استدعاءات كومة x86 (`PUSH` → `CALL EBX`) | 100% |
| رابط المرجعيات الزمني (EXEBlock patching) | 🔴 9 مواضع اقتطاع `(DWORD)` للعناوين | 100% |
| مولد صورة PE | 🔴 لا يُنتج أي صورة (PE32 ولا PE32+) | 100% |
| ABI الصفيف في Core (`DBDLLCore.cpp`) | 🔴 مقابض صفيف `DWORD` (مؤشرات 32) | ~100% |
| ثنائيات وقت التشغيل المثبّتة | 🔴 33/33 DLL هي PE32/i386 | 100% |
| `dynamic_call.asm` (MASM) | 🔴 39 سطرًا x86 بالكامل | 100% |
| مفتاح ABI الهدف `ActiveTargetAbi` | 🔴 ما زال `TargetAbi32` | 100% |

**التقدير الكلي: ما بين 5–10% فقط من المشروع أصبح 64-bit فعليًا قابلًا للتشغيل،
وما زال 90–95% 32-bit** (وقت التشغيل + الآلية + صورة PE هي الكتلة المتبقية).

### 8.2 جدول الأوبكود — تصنيف الـ 210 تعليمة (تفصيلي)

| الفئة | العدد | النسبة | أمثلة | المطلوب |
| :--- | :--- | :--- | :--- | :--- |
| A. صالحة كبايتات x64 | 179 | 85.2% | ALU بـ EAX/EBX/ECX/EDX، `[reg+disp]`، SETcc، SIB، x87، E8/0F85 | لا شيء بايتيًا؛ الدلالة تتطلب عناوين 64-bit في مواقع الاستدعاء |
| B. تحتاج REX.W | 9 | 4.3% | `ADD/SUB ESP`، `MOV EBP,ESP`، `MOV EAX,ESP`، `MOV EAX,EBP`، `MOV MEM,ESP` | إضافة `48` (REX.W) |
| C. ذاكرة مطلقة → RIP-relative | 14 | 6.7% | `MOV MEM,IMM`، `INC/DEC MEM`، `MOV EBX,[MEM]`، `FSTP/FLD [MEM]`، `JMP [MEM]` | البايتات نفسها تصبح `[rip+disp32]` — يتطلب الرابط الزمني حساب disp32 |
| D. moffs A0–A3 | 6 | 2.9% | `MOV EAX,[MEM]`، `MOV [MEM],EAX` | على x64 تصبح moffs64 (عنوان 8 بايت) أو تحويل إلى `8B 05` |
| E. غير صالحة إطلاقًا | 2 | 1.0% | `PUSHAD` (0x60)، `POPAD` (0x61) | استبدال بتسلسل دفع/سحب صريح |

### 8.3 المواقع الدقيقة لبقايا 32-bit الحرجة

**أ) محرِّك الأوبكود — `DBProCompiler/DBPCompiler/ASMWriter.cpp`**
- `GenerateASMCodes()` (سطر 93–373): جدول x86؛ المواضع 106–111 (moffs A0–A3)،
  189–191 (REX.W مفقودة)، 296–297 (PUSHAD/POPAD)، وكل `ModRM rm=101` (RIP-relative).
- `CreateASMMiddleCore()` (سطر 413): يكتب placeholder 4 بايت فقط
  (`WriteDWORD(0xFFFFFFFF, 4)` سطر 473) — لا يدعم placeholder 8 بايت.
- مهمتا `PushRegisters`/`PopRegisters` (سطر 1476–1483) تطلبان PUSHAD/POPAD.

**ب) آلية الاستدعاء — `ASMWriter.cpp` (سطر 1281–1306)**
- `ASMTask::Call`: `MOV EBX,[commandIndex]; CALL EBX` — استدعاء غير مباشر عبر جدول
  أوامر، والوسائط تُمرَّر على الكومة (`PUSHEAX`/`PUSHRELEAX` في TaskEmitter.cpp
  `WriteASMEAXtoX`، مسار `ParamMode::Stack`). على x64 يجب RCX/RDX/R8/R9 + shadow space.

**ج) الربط الزمني — `DBProCompiler/DBPCompiler/EXEBlock.cpp`**
- 9 مواضع اقتطاع عنوان إلى DWORD: سطور 1505، 1530، 1546، 1554، 1562، 1569،
  1574، 1593، 1634 (`g_pGlob->g_pMachineCodeBlock = (DWORD)m_pMachineCodeBlock;`).
- حلقة الترقيع (سطر ~1607–1625) تكتب DWORDات مطلقة — على x64 يجب حساب
  `disp32 = target − (codeBase + pos + 4)` (RIP-relative).

**د) مولد الصورة — `DBProCompiler/DBPCompiler/PEBuilder.cpp` (501 سطرًا)**
- لا يُنتج أي صورة PE؛ يجمع جداول (DLL/أوامر/سلاسل/بيانات) فقط.
- التحقق PE32+ موجود (`ValidatePE64HeaderRequirements` في PEBuilder.h سطر 46–50)
  لكن لا يوجد كاتب رؤوس/أقسام PE32+.

**هـ) وقت التشغيل — `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Core/DBDLLCore.cpp`**
- ABI الصفيف كامل بمقابض DWORD: `IsArraySingleDim(DWORD)` (225)،
  `FreeStringsFromArray(DWORD)` (2569)، `DeleteArray(DWORD)` (2652)…
- اقتطاعات مؤشرات: 2469، 2551، 2714، 3320، 3467، 3606، 5246…
- `globstruct.h`: `PTR_FuncCreateStr(DWORD*, DWORD)` — واجهة إنشاء السلاسل 32-bit.

**و) تجميع MASM — `Dark Basic Public Shared/.../System/dynamic_call.asm`**
- 39 سطرًا: `.model flat`، إطارات EBP، استدعاء كومة x86 — يحتاج إعادة كتابة x64
  (RCX/RDX/R8/R9 + shadow space). سببه الاختبار المتخطى `AssemblyCallParity`.

**ز) مفتاح الهدف — `DBProCompiler/DBPCompiler/TargetABI.h` سطر 32**
- `using ActiveTargetAbi = TargetAbi32;` — البنية الجاهزة موجودة لكن الهدف الفعلي
  ما زال معلنًا 32-bit.

**ح) ثنائيات مثبّتة — `Install/Compiler/plugins/*.dll`**
- 33/33 ملف PE32/i386 — لا يوجد DLL x64 واحد مشحون، لكن المصادر كلها متاحة (محرك 3D = مشروع `Objects`؛ راجع `docs/19_x64_basic3d_source_findings.md`).

**ط) اختبارات — `tests/` (100 ملف)**
- 6 ملفات تشير لأوبكود x86 (`test_asmwriter_emission.cpp` يتحقق من `0x60` لـ PUSHAD،
  و`test_asmwriter_enum_values.cpp`، و`test_runtime_bundle.cpp`، و`test_pe_export_inspector.cpp`…).
- `DynamicCallTest.AssemblyCallParity` متخطى على x64 بسبب `dynamic_call.asm`.

### 8.4 ما اكتمل بنسبة 100% (من الموجة الأولى)

- بناء x64 حصري مع رفض قاطع لأي Win32 (`CMakeLists.txt` سطر 11–13 + `CMakePresets.json` + CI).
- `MemoryPE` محمّل مزدوج (PE32/PE32+) مع رفض نظيف لصور 32-bit على مضيف x64.
- `CASMWriterx64` حُذف ودُمج في `CASMWriter` (أدوات REX/RIP-relative/SSE + ABI المساعدة).
- 9 مواضع `(HANDLE)0xFFFFFFFF` → `INVALID_HANDLE_VALUE` + حُرّاس NULL.
- تحويلات `DWORD`→`DWORD_PTR` في اختبارات الصفيف، و`ctest` على x64: 882 نجاحًا/0 فشل.

### 8.5 خارطة "x64 فقط بلا أي بقايا 32" (بالترتيب المطلوب)

1. **جدول الأوبكود + الإصدار** (موجة 2): REX.W للـ 9، RIP-relative للـ 14، تحويل الـ 6 moffs،
   استبدال PUSHAD/POPAD، دعم placeholder 8 بايت في `CreateASMMiddleCore`.
2. **الرابط الزمني** (EXEBlock): `uintptr_t` للمرجعيات + حساب disp32.
3. **آلية الاستدعاء** (Call task + Push task + جدول الأوامر): MS x64 ABI.
4. **ABI الصفيف في Core** + بقية `(DWORD)(ptr)` في Shared (80 موضعًا).
5. **مولد PE32+** كامل (رؤوس/أقسام/إعادة توجيه) في PEBuilder.
6. **`dynamic_call.asm`** بإصدار x64.
7. **بناء 33 DLL x64 من المصادر** (Core + Objects أولًا مع اختبار تكافؤ التصديرات) ثم قلب `ActiveTargetAbi` إلى 64.
8. **حذف كل مرجع x86** (اختبارات الـ 6 ملفات + وثائق + .vcxproj القديمة).


## 9. الموجة الثانية — إصدار الأوبكود x64 (مكتملة، مُختبرة)

**التصميم:** `docs/superpowers/specs/2026-08-11-x64-opcode-emission-design.md`
**الخطة (TDD):** `docs/superpowers/plans/2026-08-11-x64-opcode-emission.md`
**الاختبارات:** `tests/test_x64_opcode_emission.cpp` (23 اختبارًا: 19 إصدار + 4 ترقيع زمني)

### 9.1 ما أُنجز جذريًا (لا ترقيع)

1. **جدول واصفات منظم** يحل محل 4 مصفوفات متوازية (`preOp/op1/op2/bOpData`):
   كل تعليمة من الـ 210 تعلن صراحةً `DataEncoding` لحقولها: `None / Imm8 / Imm16 /
   Imm32 / Abs64 / ImmOrAddr / PtrIndirect` + `OpcodeExpansion` لـ PUSHAD/POPAD.
2. **عروض الفتحات الحقيقية**: العنوان = `sizeof(void*)` (8 بايت)، القيمة = 4 بايت —
   وكلاهما يُشتق من نفس `ParseReferenceLabel` عند الإصدار وعند الترقيع، فبقي
   **تنسيق الملف `.dbpro` دون أي تغيير** (3 مصفوفات مراجع فقط).
3. **moffs (A0-A3)**: فتحة عنوان 8 بايت كاملة (MOV EAX, [abs64]).
4. **`MOV r, imm`**: قيمة → `B8 imm32`؛ عنوان → `48 B8+rd imm64` (بدون اقتطاع).
5. **الذاكرة المطلقة `[disp32]`** (MOVEBXMEM4, MOVMEMEBX4, MOVMEMIMM*,
   MOVMEMESP4, INCMEM*, DECMEM*, FSTP/FLD [mem], JMPREL): توسعة
   `48 BB <imm64>` + نفس الأوبكود بتحويل modrm `101→011` (الوصول عبر RBX) —
   ألغت مشكلة RIP-relative مع imm الختامي نهائيًا.
6. **PUSHAD/POPAD**: توسعة إلى PUSH/POP صريحة (RAX,RBX,RCX,RDX,RSI,RDI,RBP).
7. **إصلاح أخطاء كامنة في الجدول**: SHL/SHR مع imm (C0/C1 = imm8 فعليًا)،
   نماذج mod=11 بـ `bOpData=true` وهمي (MOVEAXECX1...، PUSHRELEAX...)،
   و`MOVSIB4IMM*` (الأوبكود في preOp).
8. **الترقيع الزمني** (`CEXEBlock::PatchReferenceValues` — دالة ثابتة مُختبرة):
   الأنواع 1/2/3/6 تكتب `sizeof(void*)` بايتًا، القيم (4) تكتب 4 بايت،
   والملصقات (5) تكتب rel32 بقاعدة `pos+4`. أصلحت 8 مواضع اقتطاع مؤشرات في
   `RunProgram` (أوامر/سلاسل/متغيرات/بيانات) وتحويل `pProgramRefPtr` إلى
   `uintptr_t[]`.

### 9.2 النتيجة المقاسة

- `ctest` على `windows-x64-debug`: **903 نجاحًا / 0 فشل** (كانت 882) + تخطٍّ متوقع واحد
  (`AssemblyCallParity` — MASM x86، بند خارطة 6).
- `test_asmwriter_emission.cpp` حُدِّث: PUSHAD `0x60` → PUSH RAX `0x50`.
- بقي مؤجلًا عمدًا لموجة 3: الإطار (REX.W لـ MOV RBP,RSP)، اتفاقية الاستدعاء
  (سجلات + shadow space)، دلالة دفع 8 بايت لـ `PUSH [RBP+disp]`، و
  `g_pGlob->g_pMachineCodeBlock` (DWORD في globstruct — تغيير بنية ABI مشترك
  مع البلوقنات، يُنفَّذ مع بناء 33 DLL في البند 7).

## 10. حالة الموجة 3 — اتفاقية استدعاء Microsoft x64 (مكتملة، 2026-08-11)

حوّلنا آلية الاستدعاء في `ASMTask::Call` من دفع الوسائط على الكومة (x86)
إلى اتفاقية Microsoft x64، بمنهجية TDD (التصميم:
`docs/superpowers/specs/2026-08-11-x64-call-convention-design.md`).

### ما أُنجز

1. **إطار وسائط معلّقة** (`CASMWriter`): قائمة أنواع لكل فتحة 8 بايت تُبنى
   عند `Push`/`PushAddress` وتُستهلك عند `Call` (المزدوجات/64-bit تشغل
   فتحتين وتُعاد تجميعها: `MOV RAX,[lo]; MOV RCX,[hi]; SHL RCX,32; OR RAX,RCX`).
2. **تسلسل الاستدعاء x64**: `SUB RSP,F` → السجلات RCX/RDX/R8/R9
   (والعائمة `MOVSS` XMM0-3، والمزدوجة `MOVQ`) → نقل الوسائط 5+ إلى
   `[RSP+32+8k]` → `MOV RBX,[index]; CALL RBX` → `ADD RSP,F`.
   **F = 32 (shadow) + 8·max(0,N−4) + pad محاذاة 16**.
3. **متعقّب RSP ثابت** (mod 16، بذر 8 عند دخول البرنامج عبر استدعاء C؛
   البرولوج ذو السجلات السبعة يبدأ الجسم عند 0). الأوبكود PUSH/POP/SUBESP/
   ADDESP محدَّثة دلتا-جدولية؛ النقلات الزمنية لـ RSP (`MOV ESP,[mem]`,
   `MOV ESP,EBP`, `SUB ESP,EAX`) تسمّم المعرفة فتُسقط الاستدعاء إلى النمط
   القديم بأمان بدل محاذاة خاطئة.
4. **قمع تنظيف الكومة**: `Call` يستهلك الإطار ويُسجّل عدد فتحات الإخراج؛
   مهمات `PopEbx`/`PopEax` اللاحقة لا تُصدِر شيئًا (مطابق لحساب المستدعي:
   MathOp/ParseInstruction/ParseInit/ParseJump). القيم على مكدس IR
   (`WriteASMLine(POPEAX)`) غير متأثرة.
5. **أوبكود الإطار**: `MOVEBPESP`/`MOVESPEBP` أصبحا `48 89 E5`/`48 89 EC`
   (REX.W) ليكون RBP قاعدة إطار 64-bit.
6. **حدود آمنة**: `PushUdt` يسمّم الإطار → استدعاء قديم متوازن
   (UDT-by-value مؤجَّل للموجة 4)؛ `JumpSubroutine`/`Return`/`PushRegisters`
   يعيدون ضبط الإطار المعلّق.

### النتيجة المختبرة

- `tests/test_x64_call_convention.cpp`: **22 اختبارًا** (0/1/2/4/5/8 وسائط،
  محاذاة فردية/زوجية، عائم/مزدوج/64-bit، تداخل، قمع التنظيف، UDT، تسمّم
  RestoreEsp، أوبكود الإطار) — كلها تتحقق بايتًا ببايت.
- `ctest` على `windows-x64-debug`: **927 نجاحًا / 0 فشل** (كانت 903) +
  تخطٍّ متوقع واحد (`AssemblyCallParity`).

### مؤجَّل (موثّق في التصميم §6)

- عرض مؤشرات النصوص (32-bit) — يعتمد على تخطيط varspace x64 (موجة 4).
- تحويل x87→SSE لمجرى الأرقام العائمة (القيم تُمرَّر الآن من فتحات
  متوافقة البت عبر XMM).
- إطارات الدوال المخصصة (RBP/locals، CALLMEM) — موجة 4.
- كشف تلاعب ESP الخاص بـ Master.dll (يخزّن 32-bit) — موثق.

---

## 9. الموجة 4 — إطارات الدوال المخصصة x64 (مكتملة)

**الهدف:** تحويل إطار الدالة المخصصة بالكامل إلى x64: برولوج RBP كامل (REX.W)،
فتحات 8 بايت (كل وحدة 4 بايت في سلسلة DEC → فتحة 8 بايت)، ووصول المعاملات عبر
`[RBP+disp]` بإزاحات مضاعفة ×2، مع اختبارات TDD لكل مسار.

**الملفات المُعدَّلة:**
- `ParseUserFunction.cpp` — 4 صيغ إزاحة ×2 (معامل نصي خاص، متغير محلي، حقل UDT،
  حجم البرولوج `2*(size+4)`، حجم ClearStack `2*locals`).
- `Str.cpp` — الحالات الثلاث لترجمة `FS@` (returnvalue `-8`، معامل `2*offset+8`،
  محلي `2*(...)`).
- `ParseInstruction.cpp` — تنظيف المستدعي بعد الاستدعاء `dwMustPopStack*8`.
- `ASMWriter.cpp` — REX.W على `SUBESP`/`ADDESP` (SUB/ADD RSP،imm32)؛ وإعادة كتابة
  `ClearStack` إلى تسلسل `REP STOSB` خام (48 89 E0 / 33 C0 / 48 8B FC / B9 imm /
  FC / F3 AA) بدل حلقة SIB+LOOP القديمة.

**خلل كامن أُصلح (اكتشفه TDD):** حلقة ClearStack القديمة
(`MOV ECX,count; MOV SIB[EAX:ECX*4],0; LOOP`) تربط عدّاد الحلقة بفهرس
العنونة مع قاعدة EAX مقتطعة (32-bit) وrel8 يقفز لمنتصف التعليمة — لا يمكنها
مسح منطقة في حدودها على أي معمارية. `REP STOSB` يستخدم عدّادًا (ECX) ومقصدًا
(EDI) مستقلين تمامًا.

**التحقق (TDD أحمر → أخضر):**
- `tests/test_x64_user_function_frames.cpp` — 9 اختبارات بايتًا ببايت:
  البرولوج `55 48 89 E5 48 81 EC imm32` + ClearStack الجديد؛ الخاتمة
  `48 89 EC 5D C3`؛ REX.W على SUB/ADD RSP؛ الوصول `[RBP+16]`/`[RBP+24]`/
  `[RBP-8]`/`[RBP-16]` بعد الترقيع الزمني؛ تنظيف المستدعي ×8 (`48 81 C4 imm32`)؛
  محاذاة استدعاء DLL متداخل داخل الإطار؛ واختبار مستوى المترجم يجمّع
  `function add(a,b)` حقيقيًا عبر `MakeStatements`+`WriteDBM` ويؤكد البايتات
  المقيّسة في التدفق (`8B 85 10 00 00 00` = param0@RBP+16،
  `89 85 18 00 00 00` = param1@RBP+24).
- تحديث اختبار موجة 2 (`ADDESP` أصبح `48 81 C4` مع REX.W).
- `ctest` على `windows-x64-debug`: **936 نجاحًا / 0 فشل** (كانت 927) +
  تخطٍّ متوقع واحد.

**تخطيط الإطار x64 (المعتمد):** RBP+16+8k للمعامل k، RBP+0 للـ RBP المحفوظ،
RBP-8 لقيمة الإرجاع، RBP-16-8m للمتغيرات المحلية، و`SUB RSP, 2*(typeSize+4)`
للمساحة المحلية.

### مؤجَّل بعد الموجة 4 (موثّق في التصميم §5)

- عرض مؤشرات النصوص (varspace x64) — موجة 5.
- تحويل x87→SSE2 لمجرى الأرقام العائمة.
- معاملات UDT بالقيمة (`PushUdt` — تبقى على المسار القديم المتوازن).
- `StoreEsp`/`RestoreEsp` (32-bit) وسلامة مقارنات `_ESP_` — موثق.

## الموجة 5 — مؤشرات النصوص 64-bit (varspace + مدير النصوص) — 2026-08-11

الهدف: قيمة المتغير النصي مؤشر كومة، فيجب أن تتحرك كعنوان 8 بايت كامل على x64
في كل مسار عبر varspace، والمُصدر، وحدود ABI مع المدير النصي.

### ما نُفّذ (أحمر → أخضر → موثّق)

1. **أوبكود QWORD جديد (REX.W)** — 9 أوبكودات في `ASMOp` (240-248):
   `MOVEAXMEM8`/`MOVMEMEAX8` (48 A1/A3 + moffs64)، `MOVEAXEBP8`/`MOVEBPEAX8`
   (48 8B/89 85 + disp32)، `MOVEAXECXOFF8`/`MOVECXOFFEAX8` (48 8B/89 81)،
   `MOVEAXECXREL8`/`MOVEAXEAXREL8` (48 8B 08/00)، `MOVECXEAX8` (48 8B C8).
   `DetermineASMCall` يختارها حصريًا للنوع 3 (نص) — الأنواع الرقمية تحتفظ
   ببايتاتها 32-bit تمامًا (اختبارات عدم-انحدار).
2. **قلب ABI النشط** — اكتشفه اختبار TDD: كان `ActiveTargetAbi = TargetAbi32`
   رغم أن المُصدر x64، فكانت فتحات النصوص 4 بايت. أصبح `TargetAbi64`: الفتحات
   8 بايت، وقراءتا `ReadPointer` في `MakeVarValuesForTransfer` و`EXEBlock`
   أصبحتا بعرض المؤشر الصحيح.
3. **مدير النصوص runtime → uintptr_t** — `EquateSS`/`FreeSS`/`FreeStringSS`/
   `AddSSS`/`CreateSingleString` في DBDLLCore، و`PTR_FuncCreateStr` في
   globstruct، وكل مواقع `CreateDeleteString` في سلسلة البلوقنات (~40 موقعًا
   عبر Input/File/FTP/Memblocks/Objects/Text/System/Setup...).
4. **الأسماء المزيّنة x64** — `?EquateSS@@YAKKK@Z` → `?EquateSS@@YA_K_K_K@Z`
   و`?FreeSS@@YAKK@Z` → `?FreeSS@@YA_K_K@Z` في جدول الأوامر وكل مواضع
   `WriteASMCall` المرمّزة (ParseInit/ParseInstruction/ParseUserFunction) —
   تطابق تصدير DLL وقت التشغيل.
5. **إصلاحان قديمان يحجبان x64** — `GWL_WNDPROC`→`GWLP_WNDPROC` +
   `SetWindowLongPtr` في DBDLLCore، و`GCL_HICON`→`GCLP_HICON` +
   `GetClassLongPtr` في CGfxC.

### النتيجة

- 17 اختبارًا جديدًا (بايتًا ببايت) + اختبار ABI افتراضي كامل العرض.
- `ctest -C Debug`: **955 نجاحًا / 0 فشل** + تخطٍّ متوقع واحد.
- بناء كامل نظيف + **مصفوفة البلوقنات الكاملة (26 بلوقنًا) تبني بلا أخطاء**.

### مؤجَّل بعد الموجة 5 (موثّق في التصميم §2.5)

- **ABI الصفيف في وقت التشغيل (موجة 6)**: جدول ref في `CreateArray` يكتب
  `(DWORD)pDataPointer` — فتحات 4 بايت تقتطع عناوين العناصر على x64؛ ومعه
  سلم SIB ×8 ووصول عناصر مصفوفات النصوص. حتى ذلك الحين تبقى 103 على 32-bit
  بشكل متسق داخليًا (لا انحدار).
- x87→SSE2 لمجرى الأرقام العائمة.
- فتحات `label`/`dabel` (10/20) كعناوين 8 بايت (نظام تسميات مستقل).

## الموجة 6 — ABI الصفيف x64 (جدول ref 8 بايت + سلّم SIB ×8) — 2026-08-11

- **التصميم/الخطة**: `docs/superpowers/specs/2026-08-11-x64-array-abi-design.md` و`docs/superpowers/plans/2026-08-11-x64-array-abi.md` (TDD: أحمر 8/8 → أخضر).
- **وقت التشغيل (DBDLLCore)**: جدول ref بعناوين 8 بايت كاملة — `CreateArray` كتب `(DWORD)pDataPointer` فيفتحت العناوين؛ الآن `uintptr_t` و`* 8`. شمل كل مستهلكي الجدول: `ExpandArray`/`ReDimCore`/`FreeStringsFromArray`/`ClearDataBlock`/`EmptyArray`/`ArrayInsertAtTop`(×2)/`ArrayInsertAtElement`/`ArrayDeleteElement`، ونسختي `CMemblocks.cpp` (ميمبلوك ↔ مصفوفة). تخطيط الرأس (DWORD) لم يتغير.
- **المُصدِّر**: سلّم SIB ×8 لكل الصفائف (`MOVEAXSIB8` = `48 8B 04 D8`)؛ فتحات مؤشر الصفيف 8 بايت (MemArr/EbpArr + `CalcArrayOffset`؛ `MOVEAXMEM8`/`MOVEAXEBP8`)؛ وصول عنصر مصفوفة النصوص QWORD (`MOVECXEAXOFF8`/`MOVEAXECX8`/`MOVEAXOFFECX8` — 103 عبر `DetermineASMCallForREL`، و203 عبر `DetermineASMCall`)؛ حارس قيمة الكتابة `MOVECXEAX8` للنصوص. قيم عناصر الأعداد تبقى 4 بايت.
- **اختبارات**: `tests/test_x64_array_abi.cpp` (8) + انقلاب اختبار الموجة 5 إلى `StringArrayElementAccessUsesX64RefTable`.
- **النتيجة**: 962 نجاحًا / 0 فشل / 1 تخطٍّ متوقع؛ البناء الكامل + نواة الملحقات (إعادة تجميع قسرية لـDBDLLCore/CMemblocks) + مصفوفة الملحقات نظيفة؛ ctest 100%.

### مؤجَّل بعد الموجة 6 (موثّق في التصميم §2.4)
- **واجهة مؤشر الصفيف (موجة 7)**: دوال الصفيف المُصدَّرة تأخذ/تعيد `DWORD` و`CreateArray` ترجع `(DWORD)pArrayPtr` — اقتطاع العنوان عند حدود API وقت التشغيل (C4312). الاتساع إلى `uintptr_t` + أسماء مزخرفة + تخزين RAX كامل من المترجم.
- x87→SSE2، فتحات label/dabel، سلامة `_ESP_`.

## الموجة 7 — واجهة مؤشر الصفيف x64 (عناوين كاملة) — 2026-08-11

- **التصميم/الخطة**: `docs/superpowers/specs/2026-08-11-x64-array-api-design.md` و`docs/superpowers/plans/2026-08-11-x64-array-api.md` (TDD: أحمر 11 → أخضر).
- **وقت التشغيل (DBDLLCore)**: كل دوال الصفيف الـ33 (CreateArray/ExpandArray/DimCore/ReDimCore/DimDDD/UnDimDD/ArrayInsert*/ArrayDeleteElement/EmptyArray/Queue/Stack/Save-LoadArray/GetArrayType/IsArraySingleDim...) — معاملات وقيم رجوع المؤشر من DWORD إلى uintptr_t، وإزالة اقتطاع `return (DWORD)pArrayPtr`.
- **المترجم — الأسماء المزخرفة**: `?DimDDD@@YAKKKKKKKKKKKK@Z` → `?DimDDD@@YA_K_KKKKKKKKKK@Z` و`?UnDimDD@@YAKK@Z` → `?UnDimDD@@YA_K_K@Z` في InstructionTable وCoreRuntimeApi (typedef CoreUintptrPointer) وRuntimeBundleResolver وEXEBlock (typedef GDI_RetVoidParamUINTPTRPFN).
- **المترجم — النوع 1002**: نوع DBM مخصص "قيمة مؤشر كاملة العرض" — `DoAllocation`/`DoDeAllocation` يفرضان 1002 بدل 7، و`DetermineASMCall` يوجّهه إلى متغيرات QWORD من الموجة 5. النتيجة: دفع المؤشر القديم `48 A1`، وتخزين القيمة الراجعة `48 A3` في فتحة varspace (بدل A1/A3 المقتطعة).
- **إصلاح جذري كشفه TDD**: تخزين undim الراجع كان مفقودًا أصلًا (`SetReturnParameter("&a")` → وضع Imm → لا شيء يُصدَر). أُزيل فيمُرّ عبر المعامل الأول → `48 A3` لمسح الفتحة.
- **اختبارات**: `tests/test_x64_array_api.cpp` (6) + تحديث تثبيتي الأسماء في test_core_runtime_api/test_runtime_bundle.
- **النتيجة**: 969 نجاحًا / 0 فشل / 1 تخطٍّ متوقع؛ البناء الكامل + نواة الملحقات (إعادة تجميع قسرية) + المصفوفة نظيفة؛ ctest 100%.

### مؤجَّل بعد الموجة 7 (موثّق في التصميم §2.5)
- تعليمات القوائم (ArrayInsert/Queue/Stack) ليست في InstructionTable بعد — توقيعات وقت التشغيل توسَّعت استباقيًا.
- `&var` في كود المستخدم (قيمة عنوان متغير) تبقى type 7 (4 بايت).
- x87→SSE2، فتحات label/dabel، سلامة `_ESP_`.

## الموجة 8 — مجرى الأرقام العائمة: x87 → SSE2 في المُصدِّر (2026-08-12)

التصميم: `docs/superpowers/specs/2026-08-11-x64-sse2-float-pipeline-design.md` — TDD (أحمر 25/25 → أخضر → موثَّق).

**الحالة قبل الموجة 8**: أوبكود x87 الثمانية (FLD/FSTP عبر ST0) لتحميل/تخزين double فقط؛ حساب الأعداد العائمة كله استدعاءات DLL (`?AddFFF`/`?MulOOO`/`?EqualLOO`/…)، والمسار المرمَّز كان يرفض type 8 (`ERR_SYNTAX+50`) ويعامل float كـ DWORD؛ التحويلات int↔float كلها DLL.

**المُنفَّذ — SSE2 (XMM0 مجمّع، XMM1 معامل ثانٍ)**:
- **تحويل أوبكود x87 الثمانية في مكانها** (نفس قيم enum): FLD/FSTP → `MOVSD XMM0` بكل صيغ الذاكرة (RBX/RBP+disp/RAX+disp/RCX+disp) — تم عبر حقل `modrm` الجديد في `ASMOpcodeDef`/`DefineASM` (SSE2 memory تتطلب 4 بايت: `F2 0F 10 modrm`).
- **الحساب المرمَّز**: Add/Sub/Mul/Div للـ float/double → `ADDSS/SUBSS/MULSS/DIVSS` و`ADDSD/SUBSD/MULSD/DIVSD` — بتسلسل «حمّل B→XMM1 ثم A→XMM0» يحفظ ترتيب الطرح/القسمة، ويعيد استخدام `WriteASMXtoEAX`/`WriteASMEAXtoX` (float يُنقَل من EAX عبر `MOVD`، والنتيجة تُنسكب عبر `@$_TEMPA_`).
- **المقارنة**: `UCOMISD/UCOMISS` + بوابات `SETA/SETAE/SETB/SETBE` (علمي CF/ZF كعلمي الطرح) مع تعامل NaN الآمن (SETP/SETNP + AND/OR AL,AH) لـ EQ/NE/LT/LE.
- **التحويلات int↔float في المُصدِّر**: `CVTSI2SD/CVTSI2SS/CVTTSD2SI/CVTTSS2SI/CVTSD2SS/CVTSS2SD` — عبر BuildTask/ASMTask جدد (CastIntToFloat/… ×6) و10 إدخالات `+cast` حُوِّلت من DLL إلى builds (بقيت int64 وPower/Mod DLL — int64 أُنجز في الموجة 8ب).
- **InstructionTable**: 12 إدخال float/double (FFF/OOO: Add/Sub/Mul/Div + المقارنات الست) → `AddBuildCommand`؛ وكتلة `IncAdd/DecAdd` في ParseInstruction أصبحت توجّه إلى SSE2 Add/Sub (فكانت ستنادِي اسم DLL فارغًا).

**النتائج**: 994/0/1 (منها 26 اختبارًا جديدًا للموجة 8)؛ البناء الكامل نظيف؛ مصفوفة الـ26 ملحقًا تُبنى؛ ctest 100%.

**اكتشافات TDD**: خطأ قائم `a#=1.5` يتحطم في مسار DLL القديم؛ قيد قائم في pre-scan لإعلان متغير عادي من نوع صريح (يعمل شكل المصفوفة فقط). خارج النطاق: int64 (R) وPower/Mod تبقى DLL — int64 أُنجز لاحقًا في الموجة 8ب.

**مؤجَّل بعد الموجة 8**: تعليمات القوائم في InstructionTable؛ `&var` (4 بايت)؛ x87 المتبقي في وقت التشغيل (dbprocore — خارج المُصدِّر)؛ إصلاح تحطم `a#=1.5` وقيود pre-scan.

---

## الموجة 9 — إصلاح تحطم إسناد literal عشري لـ float (`a#=1.5`) — تشخيص جذري

**الأعراض**: `a#=1.5` يرمي استثناءً (SEH 0xC0000005 / 0xC0000027) داخل `MakeStatements` على مسار DLL القديم — أي تشغيل لمجرى التحليل دون كائن compiler حي.

**التشخيص الجذري** (مسبار SEH معزول + dumpbin RVA 0x51D06E):
- عنوان التحطم: `movzx eax, byte ptr [rax+9Bh]` داخل `CMathOp::IsLiteral` — قراءة `g_pDBPCompiler->m_bDoubleLiterals`.
- `g_pDBPCompiler` لا يُعيَّن إلا في نقطة دخول الـEXE المستقل (`Main.cpp:617`). أي مستضيف مكتبي (`dbp_compiler_lib`) يشغّل مسار التحليل دون كائن compiler حي يتحطم عند أول literal عشري — كان هذا التحطم مخفيًا خلف مسار DLL القديم (الإسناد عبر `AddFFF`/`CastLtoF`) في الموجة 8.
- بعد تحويل الموجة 8 إلى SSE2: `a#=1.5` يُصدِر تخزين float مُضمَّنًا صحيحًا (`B8 00 00 C0 3F` = `mov eax,1.5f` + `A3` store) بلا أي استدعاء DLL — فبقي التحطم فقط في `IsLiteral` بلا حارس.

**الإصلاح** (`MathOp.cpp`): قراءة آمنة للحقل `m_bDoubleLiterals` — `g_pDBPCompiler != nullptr && ...` بافتراض `false` (نفس افتراضي المُنشئ) عند غياب الكائن. لا تغيير في سلوك الإنتاج (الكائن حي دائمًا في الـEXE).

**الاختبارات** (2 جديدتان في `test_x64_sse2_math.cpp`):
- `FloatLiteralAssignmentSurvivesWithoutCompilerObject` — يُجبر حالة التحطم السابقة (`g_pDBPCompiler = nullptr`) ويُثبّت أن `a#=1.5` يمر بدون تحطم ويُصدِر `B8 00 00 C0 3F A3` بلا `AddFFF`/`CastLtoF`.
- `FloatLiteralAssignmentWithCompilerObject` — شكل الإنتاج (كائن حي) بنفس التثبيت.

**النتائج**: 996/0/1 (997 إجماليًا)؛ البناء الكامل نظيف؛ ctest 100%.

**مؤجَّل**: قيود pre-scan لإعلان متغير عادي من نوع صريح؛ تعليمات القوائم؛ `&var` (4 بايت).

---

## الموجة 10 — إصلاح قيد pre-scan: `dim d as float` (متغير عادي بنوع صريح)

**الأعراض**: `dim d as float` يفشل في pre-scan بـ `ERR_SYNTAX+43`
(`Failed to 'DoPreScanBlock(0)'`)، بينما `dim d(10) as float` و`global d as float`
و`local d as float` تعمل.

**التشخيص الجذري**: `DoDeclaration` (Statement.cpp) عند `bDoneDim==true`
يدخل دائمًا فرع `Token::Dim` الذي يستدعي `ProduceNextArrayToken` ثم
`SeperateValueFromArrayString` — وكلاهما يتطلب أقواس مصفوفة `(`. للاسم الرقمي
(بدون أقواس) يُرجع `ProduceNextArrayToken` NULL دون تحريك المؤشر، فيفشل الفصل
بـ `ERR_SYNTAX+43`. مسار GLOBAL/LOCAL لا يدخل هذا الفرع (يقرأ الاسم بمرمّز عادي)
لذلك كان يعمل. كشف الاختبار أن **كل أشكال DIM الرقمية كانت مكسورة** (عادي،
مُكتب، بسلسلة، متعدد).

**الإصلاح** (Statement.cpp): في فرع `Token::Dim`، عندما يفشل فصل الأقواس،
نعيد قراءة الاسم كمرمّز متغير عادي (`ProduceNextToken`) ونعالجه تمامًا كفرع
GLOBAL/LOCAL (`dwDecArr=0`). المؤشر مضمون عند بداية الاسم لأن
`ProduceNextArrayToken` لا يحرك مؤشر المتصل عند NULL (تحقق من Tokenizer.cpp).

**الاختبارات** (8 في `test_x64_scalar_declarations.cpp`): float مكتوبة، عادي،
integer، string، مع init (`=1.5`)، متعدد (`a as integer, b as float`)،
انحدار المصفوفة (`FF D3` CALL RBX)، وإسناد `d=1.5` يُخزَّن inline
(`B8 00 00 C0 3F A3`) بلا استدعاء DLL.

**اكتشاف TDD**: تحطم مسبار وهمي (`dg=1.5`) كان اختبارًا — `MakeStatements`
يُعدّل مخزن الإدخال (يكتب `,` مكان `=`)، فتمرير literal نصي يتحطم 0xC0000005؛
الاختبارات تنسخ البرنامج لذاكرة قابلة للكتابة.

**النتائج**: 1012/0/1 (منها 8 اختبارات جديدة)؛ البناء الكامل نظيف؛ ctest 100%.

---

## الموجة 11 — تعليمات القوائم كبناءات داخلية (ArrayInsert/ArrayDelete/Queue/Stack)

**الهدف**: تسجيل تعليمات القوائم (`ARRAY INSERT AT TOP/BOTTOM/ELEMENT`،
`ARRAY DELETE ELEMENT`، `EMPTY ARRAY`، `ADD TO QUEUE`/`REMOVE FROM QUEUE`،
`ADD TO STACK`/`REMOVE FROM STACK`) في قاعدة الأوامر الداخلية للمُصرِّف
بأسماء x64 المزخرفة (`uintptr_t` = `_K` في mangling MSVC) بدل الاعتماد على
أسماء 32-bit في موارد `DBDLLCore.rc` (`?ArrayInsertAtTop@@YAKK@Z`)، فيُصدِر
المُصرِّف استدعاءات للواجهة الموسّعة من الموجة 7.

**التشخيص الجذري**: مسار `LoadCommandsFromDLL` كان سيُصدِر استيرادًا قديمًا
غير موجود في الـ DLL الموسّع. وبقية 32-bit في مسار معامل `H` (مصفوفة كمدخل):
`Parameter.cpp` كان يفرض `m_dwType=7` (4 بايت) — على x64 يجب 1002 (8 بايت).

**الإصلاح** (جذري، غير ترقيعي):
- `InstructionTable.h`: قيم `InternalInstruction` جديدة (401–409).
- `InstructionTable.cpp`: 12 تسجيلًا عبر `AddCommandCore2` **بالأسماء الظاهرة**
  وبأسماء x64 المتحقق منها عبر dumpbin. القاعدة الداخلية تُحمَّل أولًا
  (`SetInternalInstructionDatabase` قبل `LoadInstructionDatabase`) فتصبح رأس
  سلسلة الأصدقاء وتربح `ResolveEntry` على أسماء `.rc` القديمة. دلالات
  `%H*%` (star: place=1، array-as-input) و`%H%` (plain: place=0) مطابقة للموارد.
- `Parameter.cpp`: مسار `H` يفرض `m_dwType=1002` — يصلح كل أوامر `H` وليس
  القوائم فقط.

**اكتشافات TDD**: الاسم الداخلي `+list` لا يطابق نص المصدر أبدًا (تعليمات
القوائم تُكتب صراحةً فلزمت الأسماء الظاهرة)؛ `GetRef` يحمل آخر صديق مُسجَّل
(`HL`) بينما `FindInstruction` يحلّ إلى رأس السلسلة (`H`)؛ أسماء mangling بلا
مسافات (`YAX_KH` لا `YAX_ KH`).

**الاختبارات** (9 في `test_x64_list_instructions.cpp`): جدول الأسماء x64،
مطابقة `FindInstruction` للاسم الظاهر، وبُرمج مُجمَّع لـ insert/delete/stack/
queue: دفع مؤشر الصفيف كامل العرض (`48 A1` + `50`)، استدعاء `FF D3`، تخزين
RAX الراجع كاملًا (`48 A3`) للموجبة (`H*`) بلا تخزين للخاملة (`H`).

**النتائج**: 1013/0/1 (منها 9 اختبارات جديدة)؛ البناء الكامل نظيف؛ ctest 100%.

**مؤجَّل بعد الموجة 11**: `&var` (4 بايت)؛ تحديث `DBDLLCore.rc` بالأسماء x64
(أثر نشر — القاعدة الداخلية تربح السلسلة).

---

## الموجة 12 — فك اقتران `CMathOp::IsLiteral` عن `g_pDBPCompiler` (TDD)

**الهدف**: إزالة القراءة من الكائن العام `g_pDBPCompiler->m_bDoubleLiterals`
من داخل `CMathOp::IsLiteral` نهائيًا، وتمرير تفضيل الـliteral (`bDoubleLiterals`)
كمعامل عبر سلسلة الاستدعاء من نقطة دخول التحليل (`MakeStatements`) وصولًا إلى
`IsLiteral`. الاقتران القديم كان مخفيًا في دالة تحليل نقية، ووُجد حارس
`nullptr` أصلًا كترقيع على أثر ذلك الاقتران (المترجم العام قد لا يوجد في
الاختبارات/المضيفين المضمّنين).

**الإصلاح** (جذري، غير ترقيعي):
- `CStatementList`: عضو `m_bDoubleLiterals` + `SetDoubleLiterals`/
  `GetDoubleLiterals`؛ `MakeStatements(pData, size, bDoubleLiterals=false)` و
  `AddMiniStatements(..., bDoubleLiterals=false)` يخزّنان التفضيل (سياق تحليل،
  لا قراءة من عمق).
- `CDBPCompiler::MakeProgram`: يمرر `m_bDoubleLiterals` كمعامل صريح إلى
  `MakeStatements`/`AddMiniStatements`.
- `MathOp.cpp`: توقيعات `IsLiteral(CStr*,DWORD*,bool)` و`DoValue(CStr*,bool)` و
  `DoCastOnMathOp(...,bool)` و`DoValueComplexVariable(CStr*,bool)` و
  `DoValueSingleVariable(CStr*,bool)` — المعامل يتدفق عبر كل الاستدعاءات
  الداخلية (العودية، الصب، الاشتراكات، المتغيرات المحلية). حُذف سطر قراءة
  `g_pDBPCompiler` والإعلان `extern CDBPCompiler* g_pDBPCompiler` من MathOp.cpp.
- نقاط الالتقاط الوحيدة المتبقية لقراءة السياق في طبقة `CStatement`/`CParameter`:
  `DoExpression(..., bool)` (9 مستدعي داخلي + `DoExpressionListString`)،
  `DoDeclaration`، 4 مستدعي `DoCastOnMathOp`، وموضع `IsLiteral` في معالجة CASE
  (Statement.cpp:1276) — كلها تمرر `g_pStatementList->GetDoubleLiterals()`.

**اكتشافات TDD**: موضع `IsLiteral` إضافي في معالجة `CASE` (Statement.cpp:1276)
كان خارج نطاق الجرد الأولي — الاختبارات التكاملية كشفت التوقيع القديم عبر فشل
الترجمة؛ و`test_statement_expression.cpp` يستدعي `DoExpression` مباشرة.

**النتائج**: 8 اختبارات جديدة في `test_x64_literal_preference.cpp` (وحدة
`IsLiteral`: true→8/false→2/integer→1؛ وحدة `DoValue`؛ استقلال كامل مع
`g_pDBPCompiler=nullptr`؛ وتكاملي: `MakeStatements(..., true)` يصدّر MOVSD
`F2 0F 10` بدل `mov eax,1.5f`). المجموعة 1022 اختبارًا: 1021/0/1 تخطٍّ متوقع؛
البناء الكامل نظيف؛ ctest 100%.

**مؤجَّل بعد الموجة 12**: `&var` (4 بايت)؛ تحديث `DBDLLCore.rc` بالأسماء x64
(أثر نشر — القاعدة الداخلية تربح السلسلة).

---

## الموجة 8ب — حساب int64 (type 9) REG64 كامل العرض بدل استدعاءات DLL (2026-08-12)

التصميم: `docs/superpowers/specs/2026-08-11-x64-int64-reg64-design.md` — TDD (أحمر → أخضر → موثَّق).

**الحالة قبل الموجة 8ب**: int64 ("double integer", type 9) هو النوع الحسابي
الوحيد المتبقي الذي يستدعي dbprocore.dll: `a=b+c` يقسّم القيمة نصفين 4 بايت
(EDX:EAX) ويستدعي `?AddRRR@@YA_J_J0@Z`/`?SubRRR`/`?MulRRR`/`?DivRRR`؛
المقارنات `?EqualLRR`/`?GreaterLRR`/…؛ التحميل/التخزين نصفان 4 بايت بكل
الأوضاع (Mem/MemOff/Ebp/EbpOff/Arr/Stack)؛ والقسمة على صفر تُترك للـ DLL.

**المُنفَّذ — REG64 كامل العرض**:
- **8 أوبكود REX.W جديدة** في `ASMOp`: `ADDEAXEBX8` (48 01 D8)، `SUBEAXEBX8`
  (48 29 D8)، `MULEAXEBX8` (48 0F AF D8 = IMUL RAX,RBX)، `DIVEAXEBX8`
  (48 F7 FB = IDIV RBX)، `CQO` (48 99)، `MOVEAXEDX8` (48 8B C2)،
  `MOVEBXRAX8` (48 8B D8)، `CMPEDXEBX8` (48 3B DA).
- **InstructionTable**: RRR الحسابية الأربع + مقارنات LRR الست →
  `AddBuildCommand` (BuildTask::Add/Sub/Mul/Div/Equal…LessEqual) بدل DLL؛
  وإدخال جديد `ModRRR` (كان سيسقط إلى "Not Type Based") يُوجَّه أيضًا إلى
  build — القسمة والـ mod تصدران `CQO` + `IDIV RBX` + (للمود) `MOV RAX,RDX`
  مع حارس قسمة على صفر (نفس شكل مسار int: خطأ 119) داخل المُصدِّر.
- **TaskEmitter.cpp**: كل مسارات type 9 — Mem/MemOff/Ebp/EbpOff → تحميل
  `MOVEAXMEM8`/`MOVEAXECXOFF8`/`MOVEAXEBP8` وتخزين `MOVMEMEAX8`/
  `MOVECXOFFEAX8`/`MOVEBPEAX8` (MOV 8 بايت واحد بدل نصفين 4 بايت)؛ وصول
  عنصر مصفوفة int64 عبر `MOVECXEAXOFF8`+`MOVEAXECX8` (تحميل) و
  `MOVEAXOFFECX8` (تخزين)؛ Imm int64 يكتب النصفين إلى `@$_TEMPA_`/
  `@$_TEMPB_` المتجاورين ثم `MOVEAXMEM8` واحد.
- **ABI استدعاء DLL**: int64 أصبح فتحة 8 بايت واحدة (MS x64: `__int64` يركب
  سجلًا واحدًا) — `WriteASMEAXtoX(Stack)` case 9 يدفع `PUSHEAX` واحدًا بدل
  `PUSHEDX;PUSHEAX`، و`IsDoubleSlotType` تخلّت عن 9/109 (بقيت 8/108 للنصفين
  العائمين) — فتمر المعاملات إلى RCX/RDX/R8/R9 بتحميل 8 بايت مباشر.
- **المقارنة**: `CMP RDX,RBX` (نفس ترتيب معاملات مسار int: P2→RDX عبر كومة،
  P1→RBX) + `SETE/SETNE/SETG/SETGE/SETL/SETLE` — دلالات محفوظة حرفيًا.

**النتائج**: 23 اختبارًا جديدًا في `test_x64_int64_math.cpp` (بايتات الوحدات،
مهام Add/Sub/Mul/Div/Mod/مقارنات، وتكاملي `a=b+c`/`-`/`*`/`/`/`mod`/`a=b`
بلا أي مرجع RRR/LRR)؛ اختبار call-convention حُدِّث إلى الفتحة الواحدة
(`Int64ArgGoesToRcxSingleSlot`)؛ اختبار string-pointer حُدِّث إلى التحميل
8 بايت (`Int64LoadIsSingle8ByteMove`). المجموعة 1055 اختبارًا: 1054/0/1
تخطٍّ متوقع؛ البناء الكامل نظيف؛ ctest 100%.

**خارج النطاق بوعي**: `Power` (لا تعليمة واحدة حتى للأعداد العادية — يبقى
DLL)، وتحويلات int64↔float (تبقى عبر الوسائط/الصب — مسار منفصل).

**مؤجَّل بعد الموجة 8ب**: تحويلات int64↔float داخل المُصدِّر؛ تحديث
`DBDLLCore.rc` بالأسماء x64.

## الموجة 14 — `&var` (Address-Of) عنوان 8 بايت كامل (2026-08-12)

**القيمة**: `&b` (address-of متغير عادي) كان يقرأ قيمة المرافِق `&b` بعرض 4 بايت
(`MOV ECX,[RAX+0]` + `MOV EAX,ECX`) — اقتطاع على x64. حتى الإسناد إلى int64
كان يوسِّع القيمة المقتطعة عبر استدعاء `?CastLtoR@@...` من dbprocore.dll.

**السبب الجذري**: النوعان `&b` (address-of) و`b(0)` (عنصر مصفوفة عادي) ينتجان
نفس التسمية `@&b` ونفس النوع 101 — تمييزهما مستحيل في المُصدِّر. النوع 107
("DWORD POINTER / RELATIVE ADDRESS") كان مصمَّمًا لهذا لكنه لم يُنتَج من
`DoValueSingleVariable`.

**العلاج الجذري**:
1. `CMathOp::DoValueSingleVariable` — أسماء مسبوقة بـ `&` تنتج النوع **107**
   (عرض كامل) بدل 100+القاعدة، في فرعي global و local.
2. `CTaskEmitter::DetermineASMCallForREL` + `DetermineASMCall` — النوع 107
   يُقرأ/يُكتب كـ QWORD كامل (REX.W) مثل 103/1002: `48 8B 88` (MOV
   RCX,[RAX]) + `48 8B C1` (MOV RAX,RCX).
3. `CParameter::CastAllParametersToInstruction` — قيمة 107 لا تحتاج صبًّا إلى
   أهداف العائلة الصحيحة (1/4/5/6/7/9): بلا صب = بلا استدعاء DLL.

**اكتشافات TDD**:
- `a(int64)=&b` أصبح: قراءة 8 بايت + `MOV [@a],RAX` مباشرة — **بلا أي
  استدعاء صب DLL** إطلاقًا.
- `a(integer/dword/byte)=&b`: قراءة 8 بايت ثم تخزين بعرض الهدف (الاقتطاع
  اختيار المستخدم).
- `b(0)` (عنصر مصفوفة int) بقي 4 بايت كما هو — لا توسيع.
- مُسبار التشخيص `test_x64_addressof_probe.cpp` حُذف (الاختبار التشخيصي
  `a(pointer)=&b` لم يكن يُصرَّح أصلًا — نوع "pointer" غير مدعوم في هذا
  المُجمِّع، مؤكَّد بأن `a=5` تفشل بنفس الخطأ 3:1 قبل الموجة).

**البوابة**: 6 اختبارات جديدة في `test_x64_addressof.cpp` (وحدة + مهام +
مُصرَّح end-to-end). المجموعة 1051 اختبارًا: 1050/0/1 تخطٍّ متوقع؛ البناء
الكامل نظيف؛ ctest 100%.

**خارج النطاق بوعي**: `&array(0)` عبر مساعد عنونة وقت التشغيل (يعمل أصلًا
لهدف int64)؛ PEEK/POKE في قاعدة أوامر وقت التشغيل المغلقة.

## الموجة 15 — حساب العناوين (`x=&b+n`) بعرض int64 كامل بدل DWORD (2026-08-12)

**القيمة**: بعد الموجة 14 أصبح `&b` يحمل عنوانًا كامل 8 بايت، لكن الحساب عليه
ظل في نمط DWORD (4 بايت): `a(int64)=&b+4` كان يصدّر `ADD EAX,EBX` (4 بايت)
فيتقطع العنوان، ثم يستدعي `?CastLtoR@@...` ليوسِّع النتيجة المقتطعة.

**العلاج الجذري**:
1. `CMathOp::DoValue` — أي عملية حساب فيها معامل من النوع 107 (مؤشّر
   address-of) تعمل بنمط **int64 (9)**: `ADD/SUB RAX,RBX` أصلية (موجة 8ب)،
   والنتيجة نوع 9 → `a(int64)=&b+n` إسناد مباشر 8 بايت بلا DLL. وُضعت
   القاعدة بعد قاعدة فرض DWORD (`107%100==7`) حتى يربح المؤشّر.
2. صبّات التوسيع إلى int64 أصبحت **تعليمات داخلية** (نفس نمط موجة 8):
   - `CastLToR` (int→int64، رمز 109): `MOVSXD RAX,EAX` (`48 63 C0`)
     بدل `?CastLtoR@@...` — وأغلقت فجوة الموجة 8ب (`a=int64=b+5` كان
     يستدعي DLL للـ literal).
   - `CastDTOR` (dword/107→int64، رمز 169): تمديد صفري (كتابة EAX تمسح
     النصف العلوي؛ ومصدر 107 يُقرأ 8 بايت أصلًا من الموجة 14) بدل
     `?CastDtoR@@...`.
   - التسجيل: `AddCommandCore` → `AddBuildCommand` + `BuildTask::CastIntToInt64`/
     `CastDwordToInt64` (1107/1108) + `ASMTask::CastIntToInt64`/`CastDwordToInt64`
     (607/608) + أوبكود `MOVSXDRAXEAX` (296).

**اكتشافات TDD**: ترتيب قواعد النمط حرج — قاعدة `%100` (4..7) كانت تلغي
قاعدة 107→9، فَنُقلت بعدها. المقارنات على العناوين (`r=&b>5`) تسلك مسار
`CMP RDX,RBX` + SETcc (8 بايت).

**البوابة**: 8 اختبارات جديدة في `test_x64_ptrmath.cpp` (وحدة + مهام +
مُصرَّح). المجموعة 1063 اختبارًا: 1062/0/1 تخطٍّ متوقع؛ البناء الكامل نظيف؛
ctest 100%. حُذف مسبار التشخيص `test_x64_ptrmath_probe.cpp`.

**خارج النطاق بوعي**: تضييق النتيجة إلى int (`a(integer)=&b+n`) يبقى صبًّا
DLL صحيح القيمة؛ تحويلات float↔int64 تُعالج في الموجة 16 أدناه.

## الموجة 16 — عائلة الصبّات `Cast*ToR`/`CastRto*` أصلية SSE2/REG64 (2026-08-12)

**قبل الموجة**: كل حدود int64↔float/double كانت تستدعي dbprocore.dll:
`?CastFtoR@@YA_JM@Z` (float→int64)، `?CastOtoR@@YA_JN@Z` (double→int64)،
`?CastRtoL@@YAK_J@Z` (int64→int)، `?CastRtoF@@YAK_J@Z` (int64→float)،
`?CastRtoO@@YAN_J@Z` (int64→double)، و`?CastBtoR@@YA_JE@Z`/`?CastWtoR@@YA_JG@Z`
(byte/word→int64).

**التغييرات الجذرية** (4 نقاط):
1. **أوبكود CVT بعرض 64** — `CVTTSS2SI RAX,XMM0` (`F3 48 0F 2C C0`)،
   `CVTTSD2SI RAX,XMM0` (`F2 48 0F 2C C0`)، `CVTSI2SS XMM0,RAX`
   (`F3 48 0F 2A C0`)، `CVTSI2SD XMM0,RAX` (`F2 48 0F 2A C0`). اكتشاف
   TDD حرج: **البادئة القديمة F2/F3 يجب أن تسبق REX.W** (المُصدِّر كان
   يكتب 0x48 أولًا) — أُضيف توسعة `RexWAfterPrefix` تكتب preOp → 0x48 →
   opcode → modrm.
2. **المهام**: `CastFloatToInt64` (609/1109)، `CastDoubleToInt64` (610/1110)،
   `CastInt64ToLower` (611/1111)، `CastInt64ToFloat` (612/1112)،
   `CastInt64ToDouble` (613/1113).
3. **التسجيل**: 9 أسطر من `AddCommandCore(dbprocore.dll)` إلى
   `AddBuildCommand` — `CastFTOR`/`CastOTOR` مساران SSE2 جديدان؛
   `CastBTOR`/`CastYTOR`/`CastWTOR` تشارك `CastDwordToInt64` (تمديد صفري:
   أي تحميل ≤32 بت يمسح النصف العلوي من RAX، مطابق لتوقيع `E`/`G`/`K` غير
   الموقَّع)؛ `CastRTOL`/`CastRTOD`/`CastRTOB`/`CastRTOY`/`CastRTOW`
   تشارك `CastInt64ToLower` (تحميل 8 بايت + تخزين مقتطع بعرض الهدف).
4. **الربط**: 5 خرائط BuildTask→ASMTask جديدة في `MathOp::WriteDBM`.

**اكتشافات TDD**: اسم نوع double في DBPro هو `"double float"` (وليس
`"double"`) — `dim d as double` يُنشئ int صامتًا فيذهب `r=d` إلى مسار
int→int64 بدل CVTTSD2SI؛ صُحِّحت برامج الاختبار. عدد أوبكود المُصدِّر قفز
إلى 301 (`ASMMAXCOUNT`)، فحُدِّث اختبار التوصيف `AsmMaxCountValue`.

**البوابة**: 15 اختبارًا جديدًا في `test_x64_cast_family.cpp` (وحدة بايتات
+ مهام + مُصرَّح: `r=f`، `r=d`، `f#=r`، `d=r`، `a=r`، `r=b`). المجموعة
1074 اختبارًا: 1073/0/1 تخطٍّ متوقع؛ البناء الكامل نظيف؛ ctest 100%.

**خارج النطاق بوعي**: التضييقات غير R-العائلية (`CastLToB`...) تبقى DLL
بقيم صحيحة العرض. `Power` (int/float) عُولج في الموجة 17 أدناه. بهذا، حدود
int64↔float/double في المُصدِّر كلها أصلية بلا أي استدعاء DLL.

## الموجة 17 — تعليمة Power داخلية (exp/log) للنوعين int وfloat (2026-08-12)

**قبل الموجة**: `^` على int (`PowerLLL`) وfloat (`PowerFFF`) كان استدعاءً
واحدًا معتمًا لـ dbprocore.dll (`?PowerLLL@@YAKHH@Z`/`?PowerFFF@@YAKMM@Z`) —
وهي أصلاً مجرد أغلفة رقيقة لـ CRT `pow`: `(int)pow((long double)a,(long
double)b)` / `(float)pow(a,b)`.

**العلاج الجذري** (4 نقاط):
1. **تحليل Power إلى بدائيات**: `x^y = exp(y·log(x))` — المُصدِّر الآن يملك
   الحساب كاملاً: توسيع x وy إلى double (float عبر `CVTSS2SD`، int عبر
   `CVTSI2SD`)، ثم `log` → `MULSD XMM0,XMM1` → `exp` → تضييق النتيجة
   (float عبر `CVTSD2SS`، int عبر `CVTTSD2SI` المقطوع المطابق لـ
   `(int)pow`).
2. **استدعاءات CRT مباشرة**: بدائيتا exp/log تُحلَّان عبر جدول الأوامر
   (`msvcrt.dll` نظامية مضمونة؛ `exp`/`log` تصديران غير مزخرفين). كتلة
   المُصدِّر تستدعي `AddCommandToTable("[msvcrt.dll", ",log"/",exp")` —
   تُضمَّن في EXE وتُحلَّ وقت التشغيل بـ LoadLibrary+GetProcAddress.
3. **إطارات متوازنة كليًا**: مساعد جديد `EmitTranscendentalCall` — تحميل
   الوسيط double في XMM0، `SUB RSP,frame` (ظل 32 بايت + حشو بنفس معادلة
   `EmitX64CallFrame` لمحاذاة 16 عند CALL)، `MOV EBX,[idx]; CALL EBX`، ثم
   `ADD RSP,frame`. زوج SUB/ADD ذاتي التوازن — لا يمس حالة push/cleanup
   للمكدس القيمي إطلاقًا.
4. **التسجيل**: `PowerLLL`/`PowerFFF` من `AddCommandCore` إلى
   `AddBuildCommand` مع `BuildTask::Power` (101 القائم سابقًا غير المستخدم)
   → `ASMTask::Power` (101 القائم) + كتلة إصدار جديدة. اكتشاف TDD: كانت
   قيمة `BuildTask::Power=101` موجودة ميتة في التعداد — أُعيد استخدامها بدل
   إضافة 1114.

**اكتشافات TDD**: مسار الوسيط double في المُصدِّر كامل (تحميل/تخزين MOVSD،
`MULSD`، الصبّات) من الموجات السابقة — لم يحتج سوى `MOVSD XMM1,XMM0`
الموجود (يُنسخ ثم يُحمَّل y في XMM0). لا أوبكود جديد إطلاقًا.

**البوابة**: 4 اختبارات جديدة في `test_x64_power.cpp` (مهام float/int +
مُصرَّح `a#=b#^c#` و`a=b^c`) تتحقق من تسلسل log→mul→exp وغياب أي مرجع
PowerFFF/PowerLLL ووجود msvcrt.dll و",log"/",exp" في جدولي DLL/الأوامر.
المجموعة 1078 اختبارًا: 1077/0/1 تخطٍّ متوقع؛ البناء الكامل نظيف؛ ctest
100%. حُذف مسبار التشخيص `test_x64_power_probe.cpp`.

**ملاحظة دلالية موثّقة**: `log(x)` غير معرّف لـ x≤0، فالقواعد السالبة تعطي
NaN (→ INT_MIN كـ int) حيث كان CRT `pow` يُرجع نتيجة موقّعة للأسس الصحيحة
— مطابق لدومين C exp/log. بقية العائلة (`PowerBBB/WWW/DDD/OOO/RRR`) تبقى
DLL بوعي — نفس النمط قابل للتطبيق لاحقًا.

## الموجة 18 — عائلة التضييقات `Cast*toB/W/D` أصلية (2026-08-12)

**قبل الموجة**: 13 صفًّا من صبّات التضييق كانت استدعاءات dbprocore.dll:
المصادر L/F/O/D ← الأهداف byte/word/dword (`?CastLtoB@@YAKH@Z`،
`?CastFtoB@@YAKM@Z`، `?CastOtoB@@YAKN@Z`، `?CastDtoB@@YAKK@Z`...) —
وكلها في الجوهر اقتطاعات قيم نقية (تحقّقت من DBDLLCore.cpp):
`(unsigned char)/(WORD)/(DWORD)`.

**العلاج الجذري** (3 نقاط):
1. **مهمّتان تغطيان الـ13 صفًّا**: `CastToNarrow` (614/1114) للمصادر
   الصحيحة L/D — تحميل 4 بايت ثم التخزين بعرض الهدف يفعل الاقتطاع
   (`MOV [dst],AL/AX/EAX`) مطابقًا تمامًا للصبّات C++ (وL→D/D→L نقل
   4 بايت بنفس البتات)؛ و`CastFloatToNarrow` (615/1115) للمصادر العائمة
   F/O — تحويل مقطوع `CVTTSS2SI`/`CVTTSD2SI` (نمط الموجة 8) ثم تخزين
   بعرض الهدف.
2. **التسجيل**: 13 سطرًا من `AddCommandCore(dbprocore.dll)` إلى
   `AddBuildCommand` — `CastLToB/Y/W/D` و`CastDToB/Y/W` تشارك
   `CastToNarrow`، و`CastFToB/Y/W` و`CastOToB/Y/W` تشارك
   `CastFloatToNarrow`.
3. **الربط**: خرائط BuildTask→ASMTask جديدة في `MathOp::WriteDBM`.

**البوابة**: 10 اختبارات جديدة في `test_x64_narrowing_casts.cpp` (مهام +
مُصرَّح: `b=a`، `w=a`، `d=a`، `b=f#`، `b=double`، `b=dword`) مع حراس
لا-مرجع-`CastLtoB/CastFtoB/CastOtoB/CastDtoB`. المجموعة 1088 اختبارًا:
1087/0/1 تخطٍّ متوقع؛ البناء الكامل نظيف؛ ctest 100%.

**خارج النطاق بوعي**: `CastDToL` (dword→int، نقل 4 بايت — نفس المهمة
قابلة للتطبيق بسطر لاحقًا)؛ عائلة التوسيع `CastB/Y/WTo*` تبقى DLL؛ صبّات
ومقارنات النصوص تبقى DLL (مدارة بالكومة). بهذا، عائلة `Cast*` من المصادر
الحسابية (L/F/O/D/R) إلى الأهداف الحسابية كلها أصلية — إلا `CastDToL`
الفردي.

## الموجة 19 — عائلة التوسيع `CastB/Y/WTo*` أصلية MOVZX (2026-08-12)

**قبل الموجة**: 16 صفًّا من صبّات التوسيع كانت استدعاءات dbprocore.dll:
المصادر boolean/byte/word ← الأهداف long/word/dword/float/double
(`?CastBtoL@@YAKE@Z`، `?CastWtoL@@YAKG@Z`...) — وكلها تحويلات قيمة نقية
(تحقّقت من DBDLLCore.cpp): المصادر B/Y (`unsigned char` — صفوف Y تشترك في
نفس إدخالات B) وW (`unsigned short`) تُمَدَّد **بلا إشارة**، وW→B/Y اقتطاع.

**العلاج الجذري** (4 نقاط):
1. **أوبكود MOVZX جديد**: المُصدِّر يحمّل byte/word عبر `MOV AL/AX` الذي
   يُبقي البتات العليا من EAX كما هي، فالتوسيع يحتاج صراحةً:
   `MOVZXEAXAL` (`0F B6 C0`) و`MOVZXEAXAX` (`0F B7 C0`) — صيغة تسجيلية
   واحدة تخدم كل أنماط العنونة لأنها تأتي بعد التحميل بعرض المصدر.
2. **مهمّتان تغطيان الـ16 صفًّا**: `CastWiden` (616/1116) للمصادر
   B/Y/W → الأهداف L/W/D — تحميل بعرض المصدر ثم MOVZX ثم تخزين بعرض
   الهدف؛ و`CastWidenToFloat` (617/1117) للمصادر B/Y/W → F/O — MOVZX ثم
   تحويل `CVTSI2SS`/`CVTSI2SD` (نمط الموجة 8) وتخزين.
3. **W→B/Y يبقيان اقتطاعًا** عبر `CastToNarrow` الموجود من الموجة 18
   (تحميل AX وتخزين AL صحيح تمامًا للاقتطاع).
4. **التسجيل**: 16 سطرًا من `AddCommandCore(dbprocore.dll)` إلى
   `AddBuildCommand`، وخرائط BuildTask→ASMTask جديدة في `MathOp::WriteDBM`.

**البوابة**: 12 اختبارًا جديدًا في `test_x64_widening_casts.cpp` (مهام +
مُصرَّح: `a=b`، `d=w`، `f#=b`، `dd=w`، `w=b`، `b=w`، `a=boolean`، `r=b`)
مع حراس لا-مرجع-`CastBto*/CastYto*/CastWto*` وفحص بايتات MOVZX/CVT.
المجموعة 1102 اختبارًا: 1101/0/1 تخطٍّ متوقع؛ البناء الكامل نظيف؛ ctest
100%.

**اكتشافات TDD**: (1) صيغة MOVZX التسجيلية كافية لأن التحميل بعرض المصدر
يسبقها دائمًا — لا حاجة لصيغ ذاكرة لكل نمط عنونة؛ (2) حُدِّث
`ASMMAXCOUNT` إلى 303 (كان 301) واختبار قيم التعداد المقابل؛ (3) مسارا
float/double في الموجة 8/16 (العبور عبر `@$_TEMPA_` للـ float والتخزين
المباشر `MOVSD` للـ double) أُعيد استخدامهما كما هما.

**خارج النطاق بوعي**: صبّات ومقارنات النصوص تبقى DLL (مدارة بالكومة)؛
`Dim/UnDim` و`EquateSS/FreeSS` و`ModFFF` و`Power` العائلية المتبقية
(BBB/WWW/DDD/OOO/RRR) تبقى DLL. بهذا، **عائلة `Cast*` كلها** — من وإلى
كل المصادر/الأهداف الحسابية (L/F/O/D/R/B/Y/W) — أصبحت أصلية بالكامل في
المُصدِّر بلا أي استدعاء dbprocore لصبّات القيم.

## الموجة 20 — بقية عائلة Power أصلية (PowerBBB/YYY/WWW/DDD/OOO/RRR) (2026-08-12)

**قبل الموجة**: 6 صفوف متبقية من `^` كانت استدعاءات dbprocore.dll:
byte/boolean/word/dword/double/int64 (`?PowerBBB@@YAKKK@Z`،
`?PowerWWW@@YAKKK@Z`، `?PowerDDD@@YAKKK@Z`، `?PowerOOO@@YANNN@Z`،
`?PowerRRR@@YA_J_J0@Z`) — دلالاتها تحقّقت من DBDLLCore.cpp.

**العلاج الجذري** (نقطة واحدة موسّعة): توسيع كتلة `ASMTask::Power` نفسها
من الموجة 17 (لا مهام/BuildTasks جديدة — الصفوف الستة تشترك في
`BuildTask::Power` الموجود):
1. **مصادر byte/word**: التحميل `MOV AL/AX` يُبقي البتات العليا، فصار
   التوسيع عبر `MOVZX EAX,AL/AX` (أوبكودا الموجة 19) ثم `CVTSI2SD` —
   مطابقًا للدلالة غير الموقّعة `(unsigned char)/(WORD)` + `(long double)`
   في DLL (صفوف Y تشترك في إدخال B).
2. **مصادر/أهداف int64**: `CVTSI2SD XMM0,RAX` و`CVTTSD2SI RAX,XMM0`
   بصيغة REX.W (الموجة 16) + تخزين 8 بايت — مطابقًا لـ
   `(LONGLONG)pow((double)a,(double)b)`.
3. **الهدف double**: تدوير النتيجة عبر دقة float
   (`CVTSD2SS`+`CVTSS2SD`) قبل التخزين — مطابقًا لـ
   `double result = (float)pow(a,b)` في `PowerOOO` تحديدًا.
4. **التسجيل**: 6 أسطر من `AddCommandCore(dbprocore.dll)` إلى
   `AddBuildCommand` بنفس `BuildTask::Power`.

**البوابة**: 11 اختبارًا جديدًا في `test_x64_power.cpp` (مهام byte/word/
dword/double/int64 + مُصرَّح `b=b^b`، `fl=fl^fl`، `w=w^w`، `d=d^d`،
`dd=dd^dd`، `r=r^r`) مع حراس لا-مرجع-`PowerBBB/YYY/WWW/DDD/OOO/RRR`
وفحص بايتات MOVZX وREX.W وCVTSD2SS. المجموعة 1113 اختبارًا:
1112/0/1 تخطٍّ متوقع؛ البناء الكامل نظيف؛ ctest 100%.

**اكتشافات TDD**: (1) اللامبدا `widenToDouble` وحّدت سلّم توسيع المصادر
الخمسة (float/byte/word/int64/int) في موضع واحد بدل التكرار المزدوج
لـ P1/P2؛ (2) الدقة الدلالية المهمة: `PowerOOO` ليست `pow` عادية بل
`(float)pow` — التدوير عبر float يجب أن يبقى صريحًا في المُصدِّر؛
(3) `?PowerBBB` و`?PowerRRR` كانا آخر صفّي دالة حسابية داخلية في الجدول
بخلاف `ModFFF`، فصار عدد صفوف dbprocore الكلي 36.

**خارج النطاق بوعي**: `ModFFF` (float modulo) يبقى DLL — قابل للتحويل
عبر `fmod` من msvcrt بنفس النمط لاحقًا؛ صبّات ومقارنات النصوص تبقى DLL
(مدارة بالكومة)؛ `Dim/UnDim` و`EquateSS/FreeSS` تبقى DLL. بهذا، **كل
العمليات الحسابية العددية في الجدول** (جمع/طرح/ضرب/قسمة/قوة/مقارنات/صبّات)
أصبحت أصلية في المُصدِّر بلا استدعاء dbprocore — عدا `ModFFF` وطرق
النصوص.

## الموجة 21 — ModFFF (float modulo) أصلي عبر fmod من msvcrt (2026-08-12)

**قبل الموجة**: `?ModFFF@@YAKMM@Z` كان **آخر صف حساب عددي** في الجدول
كله. دلالته في DBDLLCore.cpp: `if(fValueB==0) return 0;` ثم
`float(fmod((double)a,(double)b))`.

**العلاج الجذري** (3 نقاط):
1. **استخراج نواة الاستدعاء**: `EmitTranscendentalCall` (الموجة 17) انقسمت
   إلى `EmitAlignedCrtCall` (الإطار المحاذى 32+pad وحلّ msvcrt عبر جدولي
   DLL/الأوامر وMOV EBX;CALL EBX واستعادة SUB/ADD) + مساعد جديد
   `EmitBinaryTranscendentalCall` لاستدعاءات **بوسيطين double**
   (XMM0/XMM1) — نمط fmod.
2. **فرع Mod عائم** في كتلة Add/Sub/Mul/Div/Mod: توسيع كلا المعاملين إلى
   double (MOVD+CVTSS2SD) وسبك النتيجة إلى float عبر `@$_TEMPA_`
   (عقد الموجة 8) بعد `fmod`.
3. **حارس القسمة على صفر دقيق دلاليًا**: `AND EAX,0x7FFFFFFF` على بتات
   المقسوم عليه (يشمل `+0.0` و`-0.0`) — لكن NaN (مانتيسا غير صفرية)
   يكمل إلى fmod كما في `fValueB==0` في C تمامًا. القفزتان الأماميتان عبر
   مؤشري LeapMarker 5/6 (غير مستخدمين من آليات الحلقات/الحدود 0–4) لطول
   الكتلة المتغير (pad الإطار).

**البوابة**: 3 اختبارات جديدة في `test_x64_modff.cpp` (مهمة + مُصرَّح
`a#=b# mod c#` + برنامج مقسوم عليه متغير) مع حراس لا-مرجع-`ModFFF` وفحص
بايتات الحارس `25 FF FF FF 7F` (little-endian) وCVT وCALL EBX ووجود
`msvcrt.dll` و",fmod". المجموعة 1116 اختبارًا: 1115/0/1 تخطٍّ متوقع؛
البناء الكامل نظيف؛ ctest 100%.

**اكتشافات TDD**: (1) `EmitAlignedCrtCall` وحّدت نواة استدعاءات msvcrt
بين exp/log (وسيط واحد) وfmod (وسيطان) دون تغيير سلوك الموجة 17 — اختبارات
Power كلها بقيت خضراء؛ (2) **بايتات الحارس**: توقعت الاختبارات في البداية
`25 7F FF FF FF` (ترتيب big-endian) بينما المُصدِّر يكتب القيمة
little-endian `25 FF FF FF 7F` — كشف تشخيص البايتات أن التنفيذ كان
صحيحًا والاختبار هو الخاطئ؛ (3) حارس بتات مُجرَّد الإشارة يطابق دلالة
`==0` في C بدقة (يشمل -0.0 ويستثني NaN).

**خارج النطاق بوعي**: بهذا **انتهى كل الحساب العددي**: لا يتبقى أي صف
dbprocore للجمع/الطرح/الضرب/القسمة/القوة/الباقي/المقارنات/الصبّات.
المتبقي 35 صفًّا: طرق النصوص (`+mathstr` 14: AddSSS والمقارنات — مدارة
بالكومة)، مقارنات مؤشرات `+mathptr` (17 — تحتاج دلالات DWORD_PTR)،
وإدارة الذاكرة `Dim/UnDim` و`EquateSS/FreeSS`.
