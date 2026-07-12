# استراتيجية تحديث DarkBASIC Professional بأقل مخاطر ممكنة

## 1. الملخص التنفيذي

يحتاج المشروع إلى تحديث تدريجي عميق يشمل:

- دعم Windows 11 وx64.
- دعم Unicode بالكامل.
- تحديث المترجم وبنيته الداخلية.
- الانتقال من DirectX 9 إلى DirectX 11، ثم تقييم DirectX 12 لاحقًا.
- استبدال المكتبات القديمة أو المقيدة بـ32-bit.
- تحديث أنظمة الصوت والصور واستيراد الأصول والإدخال.
- الحفاظ قدر الإمكان على توافق مشاريع `.dbpro` و`.dba`.
- الحفاظ على قدرة FPS Creator Classic ومشاريعه الحالية على البناء والعمل.

أفضل استراتيجية ليست إعادة كتابة المشروع كله، وليست أيضًا تعديلًا مباشرًا واسعًا دون حماية. النهج الموصى به هو:

> تحديث تدريجي منضبط للمشروع الحالي، مع مسار إنتاجي واحد، واختبارات توصيف وتوافق قبل كل تغيير، ومقارنة مؤقتة بين التنفيذ القديم والجديد فقط عند استبدال المكونات شديدة الحساسية.

لا يُقصد بوجود تنفيذ قديم وجديد إنشاء مشروعين دائمين. المقارنة المؤقتة هي أداة اختبار للمكونات التي يمكن أن تغيّر دلالات اللغة أو نتائج الرسوم والتشغيل. بعد إثبات التكافؤ، يُحذف التنفيذ القديم.

---

## 2. أهداف التحديث

### الأهداف الأساسية

1. إبقاء المترجم والمشاريع الحالية قابلة للبناء أثناء التحديث.
2. الحفاظ على توافق مصدر DarkBASIC قدر الإمكان.
3. اكتشاف أي اختلاف سلوكي قبل وصوله إلى المستخدم.
4. إزالة الافتراضات المرتبطة بـ32-bit بصورة مدروسة.
5. الوصول إلى runtime ومترجم آمنين وقابلين للصيانة.
6. تحديث المكتبات دون ربط المشروع مباشرة بتفاصيلها.
7. توفير اختبارات آلية تمنع عودة الأخطاء القديمة.
8. جعل كل مرحلة قابلة للعكس والمراجعة بصورة مستقلة.

### ما يجب الحفاظ عليه

- صيغة مشاريع `.dbpro`.
- ملفات `.dba`.
- أسماء أوامر DarkBASIC وسلوكها الموثق.
- ترتيب العمليات والتحويلات التي تعتمد عليها المشاريع القديمة.
- ملفات الأصول وإعدادات المشاريع المهمة.
- السلوك الضروري لتشغيل FPS Creator Classic.

### ما لا يلزم الحفاظ عليه حرفيًا

- DLLs الثنائية القديمة ذات 32-bit.
- ABI الداخلي القديم.
- مواقع الذاكرة وأحجام المؤشرات القديمة.
- DirectX 9 وD3DX وDirectInput وDirectPlay.
- manual PE loading.
- Windows API hooks.
- المتغيرات العامة والبنى الداخلية غير الآمنة.
- ملفات التنفيذ المؤقتة وطرق التغليف القديمة إذا أمكن تحويلها بأمان.

التوافق المطلوب هو توافق مصدر وسلوك، وليس تجميد التطبيق الداخلي القديم.

---

## 3. لماذا التعديل المباشر الواسع خطر؟

المترجم يحتوي على ترابط كبير بين:

- parser.
- جداول المتغيرات والأنواع.
- code generation.
- DBM writer.
- runtime command tables.
- DLL discovery.
- executable builder.
- global mutable state.

قد ينجح البناء وتنجح اختبارات بسيطة، بينما تتغير حالة نادرة في اللغة بصورة صامتة، مثل:

- أولوية العمليات.
- تحويل integer إلى float.
- scope متغير محلي.
- suffix مثل `$` أو `#`.
- التعامل مع array أو user-defined type.
- include line mapping.
- ترتيب استدعاء أوامر runtime.
- layout لبنية تعتمد عليها الإضافات.

لذلك نجاح البناء لا يعني الحفاظ على التوافق.

المخاطر الصامتة أخطر من crash واضح؛ فقد ينتج المترجم برنامجًا يعمل لكنه يحسب نتيجة مختلفة أو يرسم المشهد بصورة غير صحيحة.

---

## 4. الاستراتيجية المعتمدة

### مشروع واحد ومسار إنتاجي واحد

المستخدم يحصل دائمًا على مترجم واحد ومسار إنتاجي معروف ومستقر.

```text
DBP/FPSC source
       │
       ▼
المسار الإنتاجي المعتمد
       │
       ▼
البرنامج الناتج
```

لا يتم تفعيل أي parser أو backend أو renderer جديد للمستخدم حتى يثبت بالتجارب أنه جاهز.

### التحديث المباشر للتغييرات منخفضة المخاطر

تُنفذ مباشرة داخل المشروع الحالي، بعد اختبارات مناسبة:

- إصلاح CMake وCTest.
- إصلاح التحذيرات وأخطاء البناء.
- إصلاح exit codes.
- إصلاح JSON output.
- استخدام RAII.
- إصلاح ownership وتسريبات الذاكرة.
- تحديث Unicode conversions.
- استبدال ANSI Windows APIs.
- إصلاح logging.
- تنظيف الملفات المشتقة من Git.
- تحديث أنواع الأحجام والمؤشرات.
- إضافة CI وsanitizers.

### مقارنة مؤقتة للتغييرات عالية المخاطر

تستخدم فقط عندما يمكن للتغيير أن يعدل سلوك البرامج:

- parser وAST.
- semantic analysis.
- type system.
- code generation.
- backend x64.
- runtime command dispatch.
- renderer الجديد.
- استيراد الأصول.
- physics أو animation.
- asset packaging.

في هذه الحالات يبقى التنفيذ القديم هو الإنتاجي، بينما يعمل الجديد داخل الاختبارات فقط:

```text
ملف الاختبار
   ├── التنفيذ القديم → النتيجة المرجعية
   └── التنفيذ الجديد → النتيجة الجديدة
                          │
                          ▼
                     مقارنة آلية
```

بعد إثبات التكافؤ:

```text
الجديد يصبح إنتاجيًا
        ↓
حذف القديم
```

لا ينبغي الاحتفاظ بالمسارين إلى أجل غير محدد.

---

## 5. منظومة الاختبارات المطلوبة

### 5.1 اختبارات توصيف Characterization Tests

قبل تعديل مكوّن قديم، يجب تسجيل سلوكه الحالي في اختبارات.

هذه الاختبارات لا تفترض أن السلوك القديم مثالي؛ بل تثبت ما تعتمد عليه المشاريع الحالية.

أمثلة:

- كيف تُحل المتغيرات غير المعرّفة؟
- هل أسماء المتغيرات case-insensitive؟
- كيف تتعامل اللغة مع overflow؟
- ما نتيجة القسمة بين integer وfloat؟
- كيف تعمل suffixes؟
- كيف تتعامل includes مع أرقام الأسطر؟
- ما ترتيب تحميل DLLs والأوامر؟

القاعدة:

> لا يُعاد تصميم مكوّن قديم قبل معرفة سلوكه الحالي آليًا.

### 5.2 اختبارات Regression بأسلوب TDD

كل خطأ يُكتشف يجب تحويله إلى اختبار يفشل أولًا:

1. كتابة اختبار يعيد إنتاج الخطأ.
2. تشغيله والتأكد من فشله للسبب المتوقع.
3. تنفيذ أقل تغيير صحيح.
4. تشغيل الاختبار والتأكد من نجاحه.
5. تشغيل جميع الاختبارات.
6. إجراء refactor مع إبقاء النتائج خضراء.

لا يُقبل إصلاح دون regression test، إلا تغييرات التوثيق أو الملفات المولدة المتفق عليها.

### 5.3 اختبارات توافق اللغة

يجب إنشاء conformance corpus يحتوي برامج `.dba` صغيرة، كل برنامج يختبر سلوكًا واحدًا:

```text
tests/conformance/
  expressions/
  variables/
  arrays/
  functions/
  types/
  strings/
  control_flow/
  includes/
  errors/
  graphics_commands/
```

كل اختبار يحدد:

- source.
- expected compile result.
- expected diagnostics.
- expected runtime output.
- expected exit code.
- expected artifacts عند الحاجة.

### 5.4 اختبارات Golden Projects

يجب استخدام مشاريع حقيقية كمرجع، أهمها:

- FPS Creator Classic.
- أمثلة DarkBASIC الرسمية.
- المشاريع المرفقة في `Install/Projects`.
- عينات plugins والألعاب القديمة.
- مشاريع تحتوي أسماء ومسارات غير لاتينية.

يتم بناء هذه المشاريع دوريًا ومقارنة:

- نجاح أو فشل الترجمة.
- diagnostics.
- الملفات الناتجة.
- runtime output.
- screenshots أو hashes مناسبة.
- logs.
- الأداء ضمن حدود متفق عليها.

### 5.5 اختبارات Differential

عند تطوير parser أو backend أو renderer جديد:

```text
نفس المدخل
  → القديم
  → الجديد
  → مقارنة normalized results
```

لا يُنصح دائمًا بمقارنة البايتات النهائية؛ backendان صحيحان قد ينتجان machine code مختلفًا. تتم المقارنة حسب المستوى المناسب:

- tokens.
- AST.
- symbol tables.
- typed IR.
- diagnostics.
- runtime output.
- rendering result.
- observable program behavior.

### 5.6 اختبارات Metamorphic

مفيدة لاكتشاف مشاكل لا نملك نتيجة متوقعة جاهزة لها.

أمثلة:

- تغيير حالة أحرف اسم المتغير يجب ألا يغير النتيجة إذا كانت اللغة case-insensitive.
- إضافة مسافات أو تعليقات لا يجب أن يغير التنفيذ.
- تغيير موقع ملف المشروع إلى مسار Unicode يجب ألا يغير النتيجة.
- إعادة ترتيب declarations المستقلة يجب ألا يغير السلوك.
- تشغيل نفس الترجمة مرتين يجب أن ينتج نتيجة حتمية.

### 5.7 Fuzzing

يجب تطبيق fuzzing على المكونات التي تقرأ مدخلات معقدة:

- lexer.
- parser.
- project reader.
- archive/package reader.
- image decoders.
- model import pipeline.
- diagnostic source mapping.

الهدف اكتشاف:

- out-of-bounds.
- integer overflow.
- hangs.
- infinite recursion.
- excessive memory allocation.
- malformed UTF-8.
- malformed assets.

---

## 6. بوابات الجودة المطلوبة

لا تُدمج أي مرحلة قبل اجتياز:

1. Clean configure.
2. Clean x64 build.
3. جميع CTest tests.
4. conformance tests.
5. golden project builds.
6. ASan configuration.
7. static analysis.
8. warnings-as-errors للكود الحديث.
9. `git diff --check`.
10. مراجعة compatibility impact.
11. توثيق أي اختلاف مقصود.
12. اختبار rollback أو تعطيل التغيير عند الحاجة.

### إعدادات مقترحة

- CMake Presets.
- Ninja أو Visual Studio generator مضبوط بصورة قابلة للتكرار.
- CTest مسجل فعليًا.
- MSVC وclang-cl في CI.
- `/W4` و`/WX` للكود الجديد.
- AddressSanitizer في debug/testing builds.
- clang-tidy أو Microsoft Code Analysis.
- dependency versions ثابتة.
- عدم تنزيل dependencies غير مثبتة أثناء كل build.

---

## 7. ترتيب التنفيذ المقترح

### المرحلة صفر: تثبيت baseline

قبل الإصلاحات الكبيرة:

- تحديد commit مرجعي.
- توثيق أدوات البناء.
- بناء x86 الحالي.
- تشغيل جميع الاختبارات الحالية.
- بناء FPS Creator Classic.
- حفظ النتائج والـlogs.
- جمع مجموعة golden projects.
- منع إدخال تغييرات واسعة في نفس commit.

هذه المرحلة ضرورية حتى نستطيع إثبات أن التحديث لم يكسر شيئًا.

### المرحلة الأولى: إصلاح أساس البناء والاختبارات

الأولوية الأولى لأنها شبكة أمان لبقية العمل:

- إصلاح clean parallel build.
- إضافة `/FS` أو ضبط PDB بصورة سليمة.
- إضافة `enable_testing()`.
- تسجيل الاختبارات عبر `add_test`.
- إضافة configure/build/test presets.
- فصل خيارات legacy عن third-party targets.
- إضافة CI x86 baseline ثم x64 لاحقًا.
- إزالة ملفات `.sbr` والملفات المشتقة.
- عدم commit binaries إلا في release artifacts.

لا يبدأ refactor كبير قبل استقرار هذه المرحلة.

### المرحلة الثانية: إزالة التغييرات التجريبية الخطرة

- تعطيل أو إزالة VFS hooks الحالية.
- إزالة MemoryPE اليدوي من مسار الإنتاج.
- إعادة AST الجزئي خارج `DoAssignment`.
- الإبقاء على parser القديم حتى وجود اختبارات تكافؤ.
- إصلاح CLI JSON وexit codes.
- عدم إعلان “complete” لميزة جزئية.

هذه المرحلة تعيد المشروع إلى baseline آمن يمكن البناء عليه.

### المرحلة الثالثة: Unicode كامل

المطلوب ليس مجرد تعريف `_UNICODE`.

التصميم:

- UTF-8 داخل compiler core وملفات source الحديثة.
- UTF-16 عند Windows APIs.
- تحويل مركزي واحد مع error reporting.
- `wWinMain` أو `CommandLineToArgvW`.
- `std::filesystem::path` للملفات.
- منع التحويل `wstring.begin()` إلى `string`.
- اختبار العربية وأسماء ملفات non-BMP وcombining characters.
- تحديد encoding للملفات القديمة وطريقة اكتشافه أو تحويله.

يجب اختبار:

- مسار تثبيت عربي.
- اسم مشروع عربي.
- أسماء ملفات `.dba` عربية.
- include path عربي.
- عنوان التطبيق والنصوص.
- diagnostics.
- asset filenames.

### المرحلة الرابعة: أمان الذاكرة والملكية

- تحديد ownership لكل pointer.
- RAII لكل HANDLE/HMODULE/file mapping/resource.
- استبدال buffers ذات الأحجام الثابتة.
- استخدام `std::span` لتمرير الذاكرة.
- استخدام `std::vector<std::byte>` للبيانات الثنائية.
- استخدام `std::string_view` فقط عندما تكون lifetime واضحة.
- فصل owning عن non-owning types.
- إزالة `new[]/delete` غير المتطابق.
- منع `const_cast` المستخدم لتغطية تصميم API خاطئ.

يجب أن تكون التحويلات صغيرة ومغطاة بالاختبارات، لا إعادة كتابة آلية ضخمة.

### المرحلة الخامسة: عزل حالة المترجم

بدل globals، تُنقل الحالة تدريجيًا إلى `CompilerSession` أو `CompilerContext`.

لكن يجب ألا يكون context مجرد نسخة من globals يعيد ربطها إليها. المطلوب:

- ownership واضح.
- lifecycle واحد.
- عدم استدعاء `Cleanup()` ثم destructor cleanup مرة أخرى.
- عدم امتلاك كائن واحد من مالكين مختلفين.
- تمرير dependencies صراحة.
- منع compiler sessions من التأثير في بعضها.

اختبارات مهمة:

- تشغيل عمليتي ترجمة متتاليتين.
- تشغيل ترجمتين متوازيتين مستقبلًا.
- فشل الترجمة الأولى لا يلوث الثانية.

### المرحلة السادسة: diagnostics وsource mapping

قبل تغيير parser:

- immutable source manager.
- `SourceId`.
- byte offsets.
- source spans.
- line index.
- include/source map.
- Unicode-aware display columns.
- JSON diagnostic schema موثق.

يجب فصل بيانات diagnostics عن terminal formatting وJSON وeditor protocol.

### المرحلة السابعة: parser وAST

لا يدخل AST إلى مسار الإنتاج حالة بحالة كما حصل في `DoAssignment`.

الترتيب الصحيح:

1. lexer موثق.
2. parser ينتج AST.
3. semantic analysis.
4. symbols/scopes.
5. type checking.
6. typed AST أو language IR.
7. code generation.

أثناء التطوير يُقارن parser الجديد بالقديم على conformance corpus. لا يصبح إنتاجيًا إلا كوحدة متماسكة، وليس fast path لبعض assignments.

### المرحلة الثامنة: الانتقال إلى x64

هذه أكبر مرحلة توافق.

يجب أولًا إزالة افتراضات:

- pointer في `DWORD`.
- cast بين pointer وinteger.
- offsets بحجم 32-bit.
- inline x86 assembly.
- calling conventions القديمة.
- struct packing غير الموثق.
- plugin ABI القديم.
- serialized pointers.

ثم تصميم backend x64.

يمكن استخدام:

- LLVM AOT، وهو الخيار الأقوى طويل المدى.
- backend x64 خاص إذا كانت قيود اللغة صغيرة، لكنه أعلى مخاطرة وصيانة.

يجب الاحتفاظ بـx86 كمرجع اختباري مؤقت حتى يحقق x64 التكافؤ، ثم يصبح x64 هو المنتج الوحيد.

### المرحلة التاسعة: runtime وplugins

يجب تعريف Runtime API واضحة ومستقرة:

- versioned interfaces.
- explicit ownership.
- x64-safe types.
- documented calling convention.
- structured errors.
- capability/version negotiation.

إضافات 32-bit لا يمكن تحميلها داخل عملية x64. الخيارات:

1. إعادة بنائها من المصدر إلى x64.
2. استبدالها بمكتبات حديثة.
3. adapter خارج العملية للحالات غير القابلة لإعادة البناء، كحل انتقالي فقط.

الأفضل هو الحفاظ على أسماء أوامر DarkBASIC وسلوكها، مع تغيير التطبيق الداخلي.

### المرحلة العاشرة: الرسوم

الانتقال المقترح:

```text
DX9 behavior characterization
       ↓
Renderer interface
       ↓
DirectX 11 implementation
       ↓
visual parity tests
       ↓
حذف DX9
       ↓
تقييم DX12 عند وجود حاجة فعلية
```

DirectX 11 أنسب كخطوة أولى لأنه:

- أقرب إلى نموذج DX9.
- أقل تعقيدًا من DX12.
- مناسب لمحرك FPS Creator Classic.
- يقلل حجم التغيير المتزامن.
- يدعم x64 وWindows 11 بالكامل.

DirectX 12 لا ينبغي اختياره لمجرد أنه أحدث؛ يحتاج إدارة صريحة للذاكرة والمزامنة والأوامر، وقد يزيد التعقيد دون فائدة واضحة للمحرك الحالي.

### المرحلة الحادية عشرة: الصوت والإدخال والأصول

كل نظام يدخل خلف واجهة يملكها المشروع.

#### الأصول

- Assimp للاستيراد العام عند الحاجة.
- تحويل الأصول إلى format داخلي موثق.
- عدم تحميل كل format مباشرة وقت التشغيل.
- اختبار transforms وmaterials وanimations.
- تحديد coordinate system ووحدات القياس بوضوح.

#### الصور

- اختيار مكتبات مصانة ومرخصة بوضوح.
- توحيد pixel formats.
- تحديد sRGB/linear behavior.
- bounded decoding.
- fuzzing للمدخلات.

#### الصوت

- XAudio2 للصوت منخفض المستوى.
- Media Foundation أو codec libraries مصانة لفك الترميز.
- فصل decoding عن mixing.
- اختبارات sample format وlooping و3D positioning.

#### الإدخال

- إزالة DirectInput القديم.
- خدمة موحدة للوحة المفاتيح والفأرة وأجهزة التحكم.
- event/state separation.
- اختبارات mapping وdead zones وhot-plugging.

---

## 8. استراتيجية منع المشاكل الصامتة

### لا تعتمد على نجاح البناء

يجب مقارنة السلوك المرئي والقابل للقياس.

### إنشاء Compatibility Manifest

لكل أمر DarkBASIC:

```text
Command name
Parameter types
Return type
Error behavior
Side effects
Thread expectations
Resource ownership
Legacy quirks
Relevant tests
Modern implementation status
```

### تصنيف الاختلافات

كل اختلاف يُصنف:

- Regression غير مقصود: يُمنع الدمج.
- Bug قديم تم إصلاحه: يحتاج توثيقًا واختبارًا.
- اختلاف ضروري لـx64 أو الأمان: يحتاج migration note.
- سلوك غير معرف: يُثبت له سلوك جديد واضح.

### Telemetry محلية للاختبارات

ليس جمع بيانات المستخدم، بل logs منظمة أثناء CI توضح:

- الأمر المنفذ.
- نوع البيانات.
- resource counts.
- loaded assets.
- diagnostic IDs.
- renderer events.

تساعد في تحديد أول نقطة اختلاف بدل الاكتفاء برسالة أن اللعبة لا تعمل.

### تشغيل اختبارات طويلة

- stress compilation.
- repeated compilation.
- long runtime sessions.
- resource create/destroy loops.
- device reset/window resize.
- asset reload.
- controller reconnect.
- malformed input suites.

---

## 9. تنظيم commits والمراجعات

كل commit يجب أن:

- يعالج مسؤولية واحدة.
- يحتوي اختبارًا مرتبطًا.
- يبني بصورة مستقلة.
- لا يجمع formatting واسعًا مع تغيير سلوكي.
- يذكر compatibility impact.
- لا يحتوي binary artifacts.
- يكون قابلًا للعكس.

صيغة مقترحة:

```text
test: characterize legacy string comparison
refactor: isolate string comparison service
fix: preserve case-insensitive Unicode comparison
```

لا يُنصح بـcommit ضخم يحمل اسم “Complete modernization” إذا كان التطبيق جزئيًا.

---

## 10. Definition of Done لكل مرحلة

تعتبر المرحلة مكتملة فقط إذا:

- المتطلبات موثقة.
- اختبارات TDD موجودة.
- شوهدت الاختبارات الجديدة تفشل قبل الإصلاح.
- جميع الاختبارات تنجح بعد الإصلاح.
- clean build ينجح.
- CTest يكتشف الاختبارات.
- golden projects تنجح.
- لا توجد تحذيرات جديدة.
- ASan لا يكتشف أخطاء.
- الفروقات المقصودة موثقة.
- legacy path المعني حُذف إذا اكتمل بديله.
- الوثائق تطابق الواقع.

---

## 11. القرار النهائي

النهج الأنسب للمشروع هو:

> تحديث تدريجي للمشروع الحالي مع مترجم ومسار إنتاجي واحد، والحفاظ على توافق مصدر DarkBASIC وFPS Creator Classic، واستخدام characterization وconformance وgolden-project وdifferential tests لمنع الانحدارات والمشاكل الصامتة.

التنفيذ القديم والجديد لا يبقيان كمنتجين دائمين. يسمح بالمقارنة المؤقتة فقط عند استبدال المكونات التي لا يمكن تحديثها بأمان في مكانها، مثل:

- backend x64.
- parser/AST.
- renderer DX11.
- runtime plugin system.

أما إصلاحات البناء وUnicode وCLI والملكية والذاكرة فتطبق مباشرة بأسلوب TDD.

### ترتيب البداية العملي

1. تثبيت baseline لمشاريع DBPro وFPS Creator.
2. إصلاح CMake وCTest وclean parallel build.
3. إضافة CI وASan والتحليل الساكن.
4. إزالة VFS/MemoryPE غير الآمنين من الإنتاج.
5. إصلاح CLI/JSON/exit codes.
6. إكمال Unicode end-to-end.
7. إصلاح ownership وحالة المترجم.
8. إنشاء source manager وdiagnostics صحيحة.
9. إعادة بناء parser/AST بصورة متكاملة.
10. تنفيذ x64 backend.
11. إعادة بناء runtime والإضافات لـx64.
12. الانتقال إلى DirectX 11.
13. تحديث الصوت والصور والإدخال واستيراد الأصول.
14. حذف مكونات x86 وDX9 بعد ثبوت التكافؤ.

هذا النهج أبطأ قليلًا في البداية، لكنه الأسرع على مستوى المشروع كله لأنه يمنع الانحدارات الصامتة وإعادة العمل، ويجعل كل تغيير قابلًا للإثبات والمراجعة والتراجع.
