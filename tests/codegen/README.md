# منظومة اختبارات مولّد الكود (DBP Codegen Test Suite)

منظومة اختبارات آلية ومؤتمتة وقابلة للتشغيل المتكرر للكومبايلر ومولّد أسمبلي x64.

تغطّي **السلامة البنيوية** لخط الإنتاج الكامل (parse → emit → relocate): تثبت أن
المولّد لا ينهار، ولا يُترك فرعاً غير محسوم، ولا يكتب خارج حدوده، وأن مخرجه حتمي
وقابل للتكرار — عبر حالات مُعَنْونة ومُعَلَّمة (parameterised) تغطي البنى اللغوية
وعائلات التعليمات والمدخلات الحدية والعدائية.

> **حدّ مهم:** المنظومة **لا** تتحقق من *صحة دلالة* الكود المُولَّد — لا تُفكِّكه
> (disassemble) ولا تُنفِّذه. مولّد يُنتج تعليمات خاطئة لكن سليمة بنيوياً سيمرّ
> كل الاختبارات. راجع قسم [ما لا تغطّيه](#ما-لا-تغطّيه) أدناه.

## ما تغطّيه

| الملف | الغرض |
|---|---|
| `CodegenHarness.{h,cpp}` | مشغّل داخلي يقود المترجم حتى بايتات الآلة دون لمس القرص؛ يلتقط البايتات + الـlisting + المراجع + التشخيصات. |
| `test_codegen_golden.cpp` | **مقارنة المخرجات المتوقعة**: كل `.dba` في `goldens/` يُقارَن وثيقةً كاملة بمخرجه المخزَّن (`.expected`). وضع التحديث: `DBP_UPDATE_GOLDENS=1`. ⚠️ **لا توجد ملفات `.expected` بعد** — انظر [ترويض الـgoldens](#ترويض-الـgoldens-bootstrap) قبل الاعتماد على هذه الطبقة. |
| `test_codegen_oracles.cpp` | **كشف القلتشات**: 10 لواطينفارنت (لا عناصر قفز غير محسومة، لا فتحات معاملات غير مُرقّعة، أهداف الفروع داخل البرنامج، الكاناري سليم، التحديد، عدم تسرب الحالة العامة…). |
| `test_codegen_edge_cases.cpp` | **اختبارات حدودية**: مدخلات غير اعتيادية وعدائية. |
| `test_codegen_headless.cpp` | **هيدليس/تسريبات**: نقع (soak) — 12 تجميعة إحماء + 80 تجميعة مقيسة لكل اختبار — مع مراقبة كومة CRT وعدد الـhandles و working set. |
| `test_codegen_instruction_matrix.cpp` | **مصفوفة التعليمات**: كل عائلة تعليمة تُولّد وتُفحص. |
| `test_codegen_language_matrix.cpp` | **مصفوفة البنى اللغوية**: if/while/for/select/function/array/UDT/… على شكل برنامج بسيط وآخر مجهَد (تعشيق/دمج). |
| `run_codegen_tests.py` | مُشغّل معزول لكل حالة اختبار (عملية منفصلة) → تقرير JSON + Markdown + HTML واضح حتى عند الانهيار. |

## كيف تُشغّل

```bash
# 1) بناء الهدف (احذف متغيرات proxy المكررة أولاً لتفادي كسر MSBuild)
unset http_proxy https_proxy HTTP_PROXY HTTPS_PROXY
cmake --build out/build/windows-x64-debug --target dbp_tests --config Debug

# 2) تشغيل منظومة codegen كاملةً مع تقرير معزول واضح
python3 run_codegen_tests.py --workers 8 --out .
```

أو عبر CTest الاعتيادي (كل الحالات في عملية واحدة — ينهي الانهيار العملية):
```bash
ctest -R dbp_tests --output-on-failure
```

## إعادة توليد الـgoldens (عند تغيّر التوليد عمداً)
```bash
DBP_UPDATE_GOLDENS=1 ./out/build/windows-x64-debug/bin/Debug/dbp_tests.exe \
  --gtest_filter='CodegenGoldens/*'
```

## ما لا تغطّيه (حدود معلومة)

المنظومة تتحقق من **سلامة البنيوية** للمُخرَج، لا من **صحته الدلالية**:

1. **لا تحقق دلالي/تنفيذي.** لا يوجد مُفكِّك (disassembler) ولا تنفيذ للكود
   المُولَّد. مولّد يُنتج `SUB` بدل `ADD`، أو يستخدم سجلاً خاطئاً، سيمرّ كل
   الاختبارات ما دام المُخرَج متسقاً بنيوياً. سدّ هذه الفجوة يتطلّب إدخال مكتبة
   تفكيك (Zydis / Capstone / iced) أو تشغيل الكود في بيئة مُحكمة — وهو **قرار
   معماري وإضافة تبعية** لم تُتَّخذ بعد.
2. **مسار كتابة الملف التنفيذي غير مغطّى**: `PEBuilder` و
   `ExecutablePreparationPipeline` و`DLLTable` والربط النهائي. الـharness يتوقّف
   عمْداً قبل أي لمس للقرص.
3. **`DBPDebugger` والإضافات (plugins)** غير مغطّاة (`loadPluginCommands=false`
   افتراضياً، أي أن أوامر الإضافات المُصدَّرة لا تُختبر).
4. **المُحسِّن واتفاقية استدعاء x64 وتخصيص السجلات** لا تُفحص كوحدات مستقلة.
5. **جودة رسائل الخطأ** (هل هي مفيدة للمستخدم؟) غير مُختبرة.

## ترويض الـgoldens (bootstrap)

طبقة الـgoldens تحتاج **خط أساس موثوق**، ولا يوجد واحد after الآن:
`goldens/` يحوي 22 ملف `.dba` وصفر ملف `.expected`، فكل حالات
`MatchesStoredGolden` تفشل حالياً بـ`golden file missing`.

**تحذير:** توليد الخط الأساس أثناء وجود عطب انهيار في المُصرِّف سيُجمّد السلوك
المعطوب كـ«متوقَّع» — وهو بالضبط ما صُمّمت هذه الطبقة لكشفه. لذا الترتيب الصحيح:

1. إصلاح عطب الانهيار في مسار الإصدار (انظر أدناه).
2. ثم `DBP_UPDATE_GOLDENS=1` مع `--gtest_filter='CodegenGoldens/*'`.
3. مراجعة `git diff` للملفات المُولَّدة يدوياً قبل اعتمادها.

## ملاحظة هامة
عند تشغيل المنظومة على بناء المترجم الحالي، تكشف **أعطالاً حقيقية** في مسار
التوليد:

- **وصول ذاكرة غير صحيح (0xC0000005 / READ)** يُسقط العملية أثناء الإصدار على
  مدخلات تافهة مثل `a = 1`. التشخيص المتاح: الهدف `0x0000002461300000` (بايتاته
  العليا ASCII قريبة من أسماء متغيّرات)، والانهيار داخل `VCRUNTIME140D.dll`
  (RVA `0x22D69`) بعنوان يقع ~1.85MB **فوق** قاعدة مكدّس الخيط المُتعطِّل — أي
  مؤشر ويلد (wild pointer) لا تجاوز كومة عادي: لهذا لا يُبلِّغ AddressSanitizer
  عن شيء. **السبب الجذري لم يُحسم بعد**؛ يحتاج مُصحِّحاً (WinDbg/cdb) أو رموز
  الـCRT أو مُحلِّل تفريغ يمشي على المكدّس.
- **فروع غير محسومة (unresolved leap placeholder)** حتى على برنامج **فارغ**
  (`MCB+39`) — وهذه هي الحالة التي يبلّغها معظم الـcorpus.
- فتحات معاملات غير مُرقّعة وتسجيلات تعليمات مكررة.

هذه مكتشفةٌ بواسطة الاختبارات تماماً كما صُمّمت لاكتشافها — إصلاح عطب الذاكرة
مهمة منفصلة في `Statement.cpp` / `ASMWriter` / `LeapMarkerManager`.

---

## Fuzzing / corpus layer (coverage expansion)

A grammar-driven fuzzing seed-generator layer discovers worse inputs than hand
-written cases can. It is wired into the same CI contract as the package fuzzers.

| File | Purpose |
|---|---|
| `fuzz/codegen_seed_generator.cpp` | Deterministic (fixed-seed) generator that writes a reproducible corpus of `.dba` files — well-formed programs plus deliberately malformed mutants (truncation, dropped terminators, NUL/Unicode injection, 20-deep IF nesting, oversized tokens, …). |
| `fuzz/dbp_codegen_fuzzer.cpp` | libFuzzer entry (Clang build) that drives `CompileSnippet()` over the corpus and traps on any universal-contract violation (unresolved branch, buffer-canary overrun). |
| `fuzz/run-codegen-corpus-smoke.ps1` | Mirrors `run-corpus-smoke.ps1`: generates the corpus, then runs the libFuzzer target (Clang) or the MSVC standalone runner. |
| `tests/codegen/CodegenCorpusRunner.cpp` | Standalone per-input driver. Launched in isolation by the Python runner so a crash in one input cannot mask the others. |
| `dbp_codegen_seed_generator` / `dbp_codegen_corpus_runner` | Tool executables built alongside `dbp_tests` (every toolchain). |

### Run the corpus mode

```bash
# 1) Build the tools (no sanitizers required)
unset http_proxy https_proxy HTTP_PROXY HTTPS_PROXY
cmake --build out/build/windows-x64-debug \
  --target dbp_codegen_seed_generator dbp_codegen_corpus_runner --config Debug

# 2) Generate the corpus + run it isolated (one process per .dba file)
python3 run_codegen_tests.py --mode corpus --workers 8 --out .

# Force regeneration of the corpus:
python3 run_codegen_tests.py --mode corpus --regenerate
```

The corpus lives in `tests/codegen/corpus/` (created on demand) with a
`manifest.json` recording each file's `kind` (valid / mutant) for triage.

