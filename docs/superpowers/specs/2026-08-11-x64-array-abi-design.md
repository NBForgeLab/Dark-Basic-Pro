# الموجة 6 — ABI الصفيف x64: جدول ref بعناوين 8 بايت + سلّم SIB ×8 + وصول عناصر مصفوفات النصوص

**التاريخ**: 2026-08-11 · **النطاق**: نواة وقت التشغيل (DBDLLCore) + مُصدِّر الأوبكود (CASMWriter/TaskEmitter) · **الأسلوب**: TDD (أحمر → أخضر → موثّق)

## 1. النموذج الحالي (32-bit) — موثّق من المصدر

### 1.1 تخطيط الصفيف في وقت التشغيل (`CreateArray`, DBDLLCore.cpp:2510)

```
[رأس 56 بايت = 14 DWORD] [جدول ref] [جدول أعلام 1 بايت/عنصر] [كتلة البيانات]
   header[0..9]  = أبعاد متتالية (خطوات التسطير)
   header[10]    = حجم الصفيف
   header[11]    = حجم عنصر البيانات
   header[12]    = نوع العنصر
   header[13]    = المؤشر الداخلي للقائمة (internal list index)
```

- `dwRefSizeInBytes = dwSizeOfArray * 4` — **جدول ref بفتحات 4 بايت**.
- `pRef[r] = (DWORD)pDataPointer` — **اقتطاع عنوان العنصر إلى 32 بت** (خلل x64 صريح).
- إزاحات الرأس من بداية جدول ref: `-16`=الحجم، `-12`=حجم العنصر، `-8`=النوع، `-4`=المؤشر الداخلي.

### 1.2 وصول العنصر في المُصدِّر

**القراءة** (`WriteASMARRtoEAX`، TaskEmitter.cpp:123):
```
MOVEAXMEM4  [arr]        ; EAX = فتحة مؤشر الصفيف  (4 بايت — اقتطاع)
CalculateArrayOffsetInEBX; EBX = الفهرس الخطي
MOVEAXSIB4               ; EAX = [EAX+EBX*4]      (سلّم ×4 — سلّم جدول ref القديم)
MOVECXEAXOFFn 0          ; ECX = [EAX+off]        (قيمة العنصر — عرض حسب النوع)
MOVEAXECXn               ; EAX = ECX              (نسخ سجل)
```
**الكتابة** (`WriteASMEAXtoARR`، TaskEmitter.cpp:385): `MOVECXEAX4` ثم `MOVEAXMEM4` ثم `SIB4` ثم `MOVEAXOFFECXn` (تخزين عبر المؤشر).

**`CalcArrayOffset`** (ASMWriter.cpp:2341): `MOVEAXMEM4`/`MOVEAXEBP4` لفتحة مؤشر الصفيف ثم حساب الفهرس الخطي.

### 1.3 عائلات العروض الحالية

- `DetermineASMCall` (TaskEmitter.cpp:6): type 3 (نص) → متغيرات QWORD من الموجة 5؛ الباقي عبر كود الحجم (0=بايت، 1=كلمة، 2=DWORD، 3=DWORDx2).
- `DetermineASMCallForREL` (TaskEmitter.cpp:42): تستخدم فقط في مسار العنصر (101-109) — **103 (مصفوفة نصوص) يقع حاليًا في DWORD**.

## 2. خريطة التحويل ×8

### 2.1 قاعدة موحّدة

> كل **فتحة عنوان** في الصفيف (جدول ref) تصبح 8 بايت على x64. عرض **قيمة العنصر** يبقى حسب نوع العنصر (نص → 8، عدد صحيح → 4، بايت → 1، كلمة → 2، عائم → مسار x87 كما هو).

### 2.2 وقت التشغيل (DBDLLCore.cpp) — كل موقع يمس جدول ref بافتراض 4 بايت

| الموقع | السطر | التغيير |
|---|---|---|
| `CreateArray` | 2510 | `dwRefSizeInBytes*4→*8`؛ `DWORD* pRef→uintptr_t*`؛ `(DWORD)pDataPointer→(uintptr_t)` |
| `FreeStringsFromArray` | 2578 | `dwRefSizeInBytes*4→*8` (حساب مؤشر البيانات) |
| `ExpandArray` | 2662 | `dwOldRefSizeInBytes/dwRefSizeInBytes *4→*8`؛ `DWORD* pOldRef/pNewRef→uintptr_t*`؛ حسابات الإزاحة `(DWORD)`→`(uintptr_t)` |
| `ClearDataBlock` | 2755 | `dwRefSizeInBytes*4→*8` |
| `ReDimCore` | 2839 | `DWORD* pOldRef/pNewRef→uintptr_t*` (فقط نوع المؤشر؛ الفهرسة تبقى) |
| `EmptyArray` | 3410 | `dwRefSizeInBytes*4→*8`؛ `DWORD* pRef→uintptr_t*`؛ `memset(...,dwRefSizeInBytes)` |
| `ArrayInsertAtTop(dw)` | 3111 | `DWORD* pRef→uintptr_t*`؛ الخلط `*4→*8`؛ `pRef[0]=dwRefItem` بعرض 8 |
| `ArrayInsertAtTop(dw,qty)` | 3153 | `DWORD* pStoreRefs→uintptr_t*`؛ نسخ `*4→*8` |
| `ArrayInsertAtElement` | 3198 | نفسه |
| `ArrayDeleteElement` | 3291 | `dwRefSizeInBytes*4→*8`؛ `DWORD* pRef→uintptr_t*`؛ `DWORD dwOffset=(DWORD)pRef[i]−...`→`uintptr_t`؛ `DWORD** pStoreRef→uintptr_t*`؛ إعادة توليد ref بـ`uintptr_t` |

- **تخطيط الرأس لا يتغير** — كل الإزاحات السالبة (`-56+dim*4`, `-16`, `-4`) تبقى DWORDs، فلا يمسّ المُصدِّر صيغها.
- حجم عنصر البيانات (`dwSizeOfOneDataItem`) يحدده `Dim` — الموجة 5 جعلته 8 للنصوص على x64. ✓

### 2.3 المُصدِّر — أوبكود QWORD جديدة + توجيه الأنواع

**أوبكود جديدة** (تُلحق بنهاية الـenum كالموجة 5، لا إعادة ترقيم):

| الأوبكود | المعنى | البايتات | التعريف |
|---|---|---|---|
| `MOVEAXSIB8` | MOV RAX, [RAX+RBX*8] | `48 8B 04 D8` | preOp=0x8B, op1=0x04, op2=0xD8, RexW |
| `MOVECXEAXOFF8` | MOV RCX, [RAX+disp32] | `48 8B 88 disp32` | op1=0x8B, op2=0x88, Imm32, RexW |
| `MOVEAXECX8` | MOV RAX, RCX | `48 8B C1` | op1=0x8B, op2=0xC1, RexW |
| `MOVEAXOFFECX8` | MOV [RAX+disp32], RCX | `48 89 88 disp32` | op1=0x89, op2=0x88, Imm32, RexW |

**TaskEmitter.cpp**:
- `WriteASMXtoEAX`: MemArr `MOVEAXMEM4→MOVEAXMEM8`؛ EbpArr `MOVEAXEBP4→MOVEAXEBP8` (فتحة مؤشر الصفيف).
- `WriteASMEAXtoX`: MemArr/EbpArr — حارس القيمة `MOVECXEAX4→MOVECXEAX8` للنصوص (3/103/203)؛ فتحة المؤشر `MOVEAXMEM4→8`/`MOVEAXEBP4→8`.
- `WriteASMARRtoEAX`/`WriteASMEAXtoARR`: `MOVEAXSIB4→MOVEAXSIB8` (لكل الصفائف — سلّم الجدول موحّد ×8).
- `DetermineASMCallForREL`: `case 103` → `MOVECXEAXOFF8`/`MOVEAXECX8`/`MOVEAXOFFECX8` (قيمة عنصر النص 8 بايت).
- `DetermineASMCall`: `case 203` (مرجع نسبي لعنصر نص) → متغيرات QWORD الموجودة من الموجة 5 (فتحة المؤشر + فك الإشارة).

**ASMWriter.cpp** — `CalcArrayOffset` (السطر 2341): `MOVEAXMEM4→MOVEAXMEM8`؛ `MOVEAXEBP4→MOVEAXEBP8`.

**سلامة قفزات الحدود**: `CLeapMarkerManager` ترقّع قفزاتها الأمامية خلفيًا (Backpatch) بأطوال فعلية من المواضع المسجّلة — نمو سلّم SIB/العنصر لا يكسرها. كتلة خطأ `JMP leapstring` الثابتة لا تتغير (حجمها مستقل عن الموجة 6). ✓

### 2.4 القرارات المؤجّلة (خارج الموجة 6)

- **واجهة مؤشر الصفيف `DWORD dwArrayPtr` في وقت التشغيل (موجة 7)** — دوال الصفيف
  المُصدَّرة (CreateArray/ExpandArray/ReDimCore/…) تأخذ وتعيد `DWORD`، و`CreateArray`
  ترجع `(DWORD)pArrayPtr` — اقتطاع 32 بت للعنوان عند حدود الـAPI (ويتجلى كتحذير
  C4312 في `CMemblocks.cpp:1064`). سلّم ×8 وجدول ref المنجز هنا صحيحان؛ عندما
  تتّسع الواجهة (uintptr_t + أسماء مزخرفة جديدة + تخزين RAX كامل من المترجم)
  يكتمل السلسلة. حتى ذلك الحين: متسق داخليًا، لا انحدار.
- **مصفوفات UDT بالقيمة** — مرور معاملات UDT كقيم (موجود كسمّ poison من الموجة 3).
- **عروض REL 201/204/206** — فتحات المؤشرات النسبية لغير النصوص تبقى 4 بايت مؤقتًا (عرض متسق داخليًا على x64).
- x87→SSE2، سلامة `_ESP_`، فتحات label — كما في الموجات السابقة.

### 2.5 اكتشافات TDD أثناء التنفيذ

- **حارس قيمة الكتابة** في `WriteASMEAXtoX` (MemArr/EbpArr) كان مرمّزًا `MOVECXEAX4`
  بلا نوع — قلُّص مؤشر النص إلى 32 بت عند تخزين `arr$[i]=...`. أصبح
  `MOVECXEAX8` للنصوص (3/103/203) والسلوك القديم للبقية.
- **وجهة `CalcArrayOffset`** تذهب عبر `ImmOrAddr` — يصدر المترجم `48 B9 + imm64`
  لعنوان الوجهة (لا اقتطاع) ثم تخزين DWORD للفهرس الخطي؛ لم يتطلب الموجة تغييرًا.
- **قفزات حدود الصفيف** (LeapMarker) تُرقَّع خلفيًا بأطوال فعلية — نمو سلّم SIB
  من 3 إلى 4 بايت لم يكسرها (تحقق بالاختبارات البايتية مع علم الفحص مُفعّل في
  الموجة السابقة).

## 3. التحقق

- اختبارات بايتًا ببايت (على مستوى المُصدِّر): قراءة عنصر مصفوفة نصوص (سلسلة كاملة `48 A1 ... 48 8B 04 D8 ... 48 8B 88 ... 48 8B C1`)، كتابة عنصر (`48 8B C8 48 A1 ... 48 89 88`)، مصفوفة أعداد صحيحة (SIB ×8 + قيمة 4 بايت)، فتحة مؤشر عبر EBP، مسار CalcArrayOffset، ومرجع نسبي لنص (203).
- وقت التشغيل لا يُربط في اختبارات الوحدة (الاختبارات تربط `dbp_compiler_lib` فقط) — يُتحقق ببناء `dbp_plugin_core` (مع إعادة تجميع DBDLLCore.cpp/CMemblocks.cpp قسريًا) ومصفوفة الملحقات.
- اختبار الموجة 5 `StringArrayElementAccessStays32BitForNow` انقلب إلى عقد ×8 (`StringArrayElementAccessUsesX64RefTable`).
- **النتيجة**: 962 نجاحًا / 0 فشل / 1 تخطٍّ متوقع؛ البناء الكامل ونواة الملحقات والمصفوفة كلها نظيفة؛ ctest 100%.
