# اكتشافان تصحيحيان: مصدر محرك 3D موجود، وبناء x64 مثبت

> وثيقة أدلة مبنية على فحص فعلي للمستودع وGitHub (أغسطس 2026).
> **تصحّح** ادعاءً سابقًا في `docs/17_x64_only_transition_research.md` كان يرى أن
> محرك الرسوم ثلاثية الأبعاد (`Basic3D` / `DBProBasic3DDebug.dll`) **بلا مصدر** —
> وهو الادعاء الذي اعتُبر «أهم عائق» أمام التحول الكامل إلى x64. هذا الادعاء
> **خاطئ**، والوثيقة أدناه تثبت ذلك، وتضيف دليلًا ثانيًا من سلالة GameGuru MAX
> على أن المحرك يُبنى x64 فعلًا.

---

## 1. الخلاصة

1. **مصدر محرك 3D موجود في هذا المستودع**: المحرك ليس كيانًا مفقودًا منفصلًا، بل هو
   **مشروع `Objects`** (`DarkSDK/Objects/Objects.vcxproj`) الذي يُنتج
   `DBProBasic3DDebug.dll`. البحث السابق فاته لأن المشروع يُسمّى "Objects" لا
   "Basic3D".
2. **تكافؤ التصديرات شبه كامل**: الملف الثنائي المشحون (بناء 2015) يصدّر **782 دالة**؛
   من الأسماء الفريدة البالغة **709**، توجد **708** (99.86%) في مصدر المستودع.
3. **بناء x64 مثبت من سلالة أخرى**: مستودع GameGuru MAX يحوي نسخة أحدث من نفس
   "Dark Basic Public Shared"، مبنية **x64 حصريًا** (v143/VS2022) وكـ **مكتبات ثابتة
   مبنية فعلًا** (`Lib64/Release/Basic3D.lib` وغيرها — تحقّقنا من معمارية `8664
   machine (x64)`)، مع تحفظات (مكتبات ثابتة لا DLLs، والمصنوعات غير مرفوعة على
   GitHub، والكود ما زال يحمل افتراضات 32-بت).

**الأثر الاستراتيجي**: «العائق الحاسم» في خارطة الطريق السابقة **غير موجود**؛
التحول الكامل إلى x64 لم يعد معلّقًا على قرار خارجي (مصدر/ترخيص) بل على عمل
هندسي قابل للتنفيذ من المصادر المتاحة.

---

## 2. الاكتشاف الأول — المحرك هو مشروع `Objects` (المصدر موجود)

### 2.1 سلسلة الأدلة

| الدليل | التفصيل |
| :-- | :-- |
| **مخرج المشروع** | `DarkSDK/Objects/Objects.vcxproj` يصرّح `OutputFile` كـ `DBProBasic3DDebug.dll` — وحتى بمسارات تثبيت: `...\FPS Creator\DBProBasic3DDebug.dll` و`...\Game Guru\DBProBasic3DDebug.dll` — أي أن هذا المشروع الواحد هو محرك 3D المشترك بين DBPro وFPS Creator وGameGuru |
| **الملفات المجمَّعة** | `Shared/Objects/CObjectsC.cpp` (13,160 سطرًا — يعرّف `MakeObject`، `AddLimb`، `MakeObjectSphere`…)، `CObjectManagerC.cpp`، `CPositionC.cpp`، `CommonC.cpp`، أنظمة التصادم (`BoxCollision`/`ElipsoidCollision`)، `DBOFormat` (تحميل الشبكات)، `ShadowMapping`، `Occlusion`، `cLightMaps`، `BSPTree`، `CSG`، `Universe`… |
| **نفس الأسماء المزخرفة** | سجل أوامر `Objects.rc` يثبّت `MAKE OBJECT%LLL%?MakeObject@@YAXHHH@Z` — وهو نفس الاسم الذي تحلّه بلوقنات أخرى من `g_Basic3D` عبر `GetProcAddress` (مثل `BulletPhysics.cpp:267`) |
| **المستودع الرسمي** | شجرة `Dark-Basic-Software-Limited/Dark-Basic-Pro` (فرع `Initial-Files`، 5,953 ملفًا) مطابقة: `DarkSDK/Objects` موجودة، ولا يوجد أي `Basic3D.vcxproj` — لأنه ببساطة اسمه Objects |
| **GameGuru (شاهد إضافي)** | `DarkSDK/Objects/x64/Release/Basic3D.lib.recipe` و`Basic3D.pdb` — سلالة GGMAX أعادت تسمية الهدف إلى `Basic3D` صراحةً |

### 2.2 التحقق الكمي — مطابقة التصديرات

المنهجية (على نسخة ويندوز، عبر `dumpbin`):

1. `dumpbin /EXPORTS Install/Compiler/plugins/DBProBasic3DDebug.dll` → **782 دالة
   مصدَّرة** (مثل `?AddLimb@@YAXHHH@Z`، `?MakeObject@@YAXHHH@Z`).
2. استخراج أسماء الدوال الأساسية (فكّ الاسم المزخرف: ما قبل `@@`).
3. فحص كل اسم في مجلدات المصدر:
   `Shared/Objects`، `Shared/DBOFormat`، `Shared/Core`، `Shared/Error`،
   `Shared/MemoryManager`، `DarkSDK/Objects`.

**النتيجة: 708 من 709 اسمًا فريدًا موجودة في المصدر (99.86%)** — والاسم الوحيد
"الناقص" (`GetObjectA`) هو دالة Windows API (GDI) مستوردة لا مصدَّرة من المحرك.

### 2.3 تحفظات

- الثنائي المشحون بتوقيت **2015**؛ المصدر المنشور من سلالة DBPro 7.x — التكافؤ
  708/709 يقوّي أن المصدر هو نفس السلالة تقريبًا، لكن لا يثبت التطابق البايتي.
- المشروع يبني على **DirectX SDK القديم (أغسطس 2007)** (`d3d9.h`، `D3DX9`) — يجب
  توفيره في أي بناء x64 (المستودع الحالي يمرره عبر `DXSDK_INCLUDE_DIR` في CMake).

---

## 3. الاكتشاف الثاني — GameGuru MAX بنى المحرك x64 فعلًا

### 3.1 ما وجدناه

| العنصر | النتيجة |
| :-- | :-- |
| **الرابط في المستودع الرسمي** | `GameGuru Core/Dark Basic Public Shared/Dark Basic Pro SDK` موجود في شجرة GitHub الرسمية لـ `Dark-Basic-Software-Limited/GameGuruMAX` (7,014 ملفًا) |
| **إعدادات المشروع** | `DarkSDK/Objects/Objects.vcxproj` فيه `Debug\|x64` و`Release\|x64` **حصريًا** (بلا Win32) وtoolset **v143 (VS 2022)** |
| **نوع المخرَج** | `StaticLibrary` (وليس DLL) — يُخرج `Basic3D.lib` في `..\..\..\Lib64\{Debug,Release}` |
| **مصنوعات مبنية فعلًا** | `GameGuru Core/Dark Basic Public Shared/Lib64/Release/` يحتوي **25 مكتبة**: `Basic3D.lib`، `DBDLLCore.lib`، `Animation`، `Basic2D`، `Bitmap`، `Bullet`، `CPU3D`، `Camera`، `ConvX`، `DarkAI`، `DarkLUA`، `Enhancements`، `FTP`، `File`، `GGVR`، `Image`، `InfiniteVegetation`، `Input`، `Light`، `Memblocks`، `PhotonMultiplayer`، `Setup`، `Sound`، `Sprites`، `System`، `Text`، `Vectors` |
| **التحقق من المعمارية** | `dumpbin /HEADERS Lib64/Release/Basic3D.lib` → **`8664 machine (x64)`** (28 كائنًا) |
| **سكربتات البناء** | `build_two_libs.bat` يستدعي `msbuild Objects.vcxproj /p:Configuration=Release /p:Platform=x64 /t:Rebuild` — نجح (سجلات `build_darksdk_*.log` تحوي تحذيرات فقط) |

### 3.2 تحفظات حاسمة

1. **مكتبات ثابتة، لا DLLs**: GameGuru MAX يربط كل وقت التشغيل داخل تنفيذي واحد،
   فلا وجود لـ `DBProBasic3DDebug.dll`/`DBProCore.dll` كملفات هناك. هدفنا (تطبيقات
   مضمّنة داخل FPSC عبر `LoadLibrary`) يتطلب DLLs — لكن إثبات نجاح البناء x64
   للمصدر نفسه يكفي دليلًا على إمكانية DLL x64.
2. **المصنوعات غير مرفوعة على GitHub**: `Lib64/` ومجلدات `x64/` المبنية موجودة في
   النسخة المحلية فقط؛ شجرة GitHub الرسمية لا تحتوي أي `.lib`.
3. **الكود ليس منقّى 64-بت**: سجلات البناء تعرض `C4267` (`size_t → DWORD`) و`C4244`
   — أي أن السلالة تُجمَّع x64 «بما يعمل» مع بقاء افتراضات 32-بت (مقابض DWORD،
   تقليصات مؤشرات). `globstruct.h` عندهم (`SDK/DirectX/globstruct.h`) ما زال بعرض
   32-بت.
4. **السلالتان تفرّقتا**: `CObjectsC.cpp` عندهم 11,242 سطرًا مقابل **13,160** عندنا؛
   `DBDLLCore.cpp` عندهم 6,286 مقابل **5,593** عندنا (أضافوا أوامر GameGuru MAX) —
   أي أن «السحب الحرفي» ليس خيارًا؛ الدمج يحتاج مقارنة وهندسة.

---

## 4. العوائق الحقيقية المتبقية (بعد التصحيح)

لم يعد بينها «مصدر محرك مفقود». الباقي قابل للتنفيذ:

1. **إكمال المترجم x64**: الموجات 1–21 منجزة؛ المتبقي 22–26 (مقارنات مؤشرات
   `mathptr`، مقارنات نصوص، `AddSSS`، مسارات 4 بايت في `PushInternalArrayIndex`
   ودفع double) — يراجعها `docs/18_x64_conversion_backlog.md`.
2. **جرد عرض المؤشرات في وقت التشغيل**: تحويل افتراضات 32-بت إلى عرض مؤشر صحيح في
   ~40 بلوقنًا (بدأ: الموجتان 5–6 في Core؛ الباقي في Objects وDBOFormat وسواهما).
3. **بناء مجموعة DLLs x64**: تحويل مشاريع `DarkSDK/*` (33 DLL) إلى x64 من مصادرها
   — بدءًا بـ `DBDLLCore` و`Objects` (المحرك) — مع **اختبار تكافؤ تصديرات** مقابل
   الثنائيات المشحونة.
4. **التبعية على DirectX SDK القديم** وبلوقنات مغلقة/مستثناة: `ODE`/DarkPHYSICS
   (مصدر موجود) ،DarkLIGHTS ،GameFX (مرخّص) ،المحوِّلات (Conv3DS/MD2/MD3/MDL/X —
   نسخ موزَّعة ثنائية) ،`Multiplayer` (مستثنى من CMake لاعتماد DirectPlay قديم).
5. **PE32+ كامل + معلومات الإزاحة (unwind)** لإخراج برامج مولَّدة x64.

---

## 5. خارطة الطريق المصحَّحة (ملخص)

| المرحلة | المحتوى | الحالة |
| :-- | :-- | :-- |
| **1** | إكمال المترجم x64 (موجات 22–26 + PE32+ كامل) | قيد التقدم — لا عائق خارجي |
| **2** | جرد عرض المؤشرات في Shared (~40 بلوقنًا) | بدأ في الموجات 5–6 |
| **3** | بناء DLLs x64 من المصادر (Core + Objects أولًا) مع اختبار تكافؤ التصديرات — محرك 3D **مصدره متاح** وإثبات نجاح البناء x64 من سلالة GGMAX | قابل للتنفيذ الآن |
| **4** | دمج أوامر سلالة GameGuru MAX عند الحاجة (مقارنة سلالات موثقة) | اختياري |
| **5** | إسقاط x86 نهائيًا بعد معايير الجاهزية (conformance x64 + توفر x64 لكل DLL محمَّل) | لاحقًا |

لم يعد التحول الكامل إلى x64 معلّقًا على «الحصول على مصدر Basic3D»؛ القرار
الوحيد المتبقي إجرائي: **متى** (وليس **هل**) نبني مجموعة DLLs x64.

---

## مراجع داخل المستودع

- `docs/17_x64_only_transition_research.md` — الوثيقة المُصحَّحة (ادّعاؤها السابق خاطئ).
- `docs/18_x64_conversion_backlog.md` — صفوف dbprocore المتبقية وموجات المترجم.
- `Dark Basic Public Shared/Dark Basic Pro SDK/DarkSDK/Objects/Objects.vcxproj` — مشروع المحرك.
- `Dark Basic Public Shared/Dark Basic Pro SDK/Shared/Objects/CObjectsC.cpp` — تنفيذ المحرك.
- `Install/Compiler/plugins/DBProBasic3DDebug.dll` — الثنائي المشحون (782 تصديرًا).
- النسخة المحلية `D:\GitHub-repo\GameGuruMAXRepo\GameGuruMAX\GameGuru Core\...` — سلالة GGMAX و`Lib64/`.
