# Phase 4 Modernization: CVarTable, CLabelTable, and CStructTable STL Modernization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Modernize the lookup dictionaries of `CVarTable`, `CLabelTable`, and `CStructTable` in the compiler, replacing the legacy x86-only `db3::TDictionary` library dependency with standard C++ `std::unordered_map` container lookups, ensuring full memory cleanup across compilation sessions.

**Architecture:** Use `std::unordered_map<std::string, T*>` to map string names to symbol tables. Implement a static case-insensitive string transformation utility `to_lower` to retain the case-insensitivity behavior of the original compiler. Clear the map lookups during `Free()` to ensure zero memory leaks and state pollution between separate runs of the compiler context.

**Tech Stack:** C++17, Win32 API, CMake, GoogleTest.

---

### Task 1: Modernize Variable Table (`CVarTable`) Dictionary

**Files:**
- Modify: `DBProCompiler/DBPCompiler/VarTable.h`
- Modify: `DBProCompiler/DBPCompiler/VarTable.cpp`
- Modify: `tests/test_vartable.cpp`

- [ ] **Step 1: Write a unit test verifying variable lookups and Free() cleanups**

  Modify [test_vartable.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/tests/test_vartable.cpp) to verify that lookups remain case-insensitive and that `Free()` clears all variables:
  ```cpp
  TEST(VarTableTest, DictionaryCaseInsensitiveLookupAndFree) {
      CVarTable* pHead = new CVarTable("$_ESP_");
      
      DWORD dwAction = 0;
      bool bRes = pHead->AddVariable("MyIntegerVar", "integer", 0, 10, true, &dwAction, false);
      EXPECT_TRUE(bRes);
      
      // Case-insensitive find
      CVarTable* pFound = pHead->FindVariable("", "myintegervar", 0);
      ASSERT_NE(pFound, nullptr);
      EXPECT_STREQ(pFound->GetVarName()->GetStr(), "MyIntegerVar");
      
      // Clean up and assert lookups are cleared
      pHead->Free();
      
      CVarTable* pFoundAfterFree = pHead->FindVariable("", "myintegervar", 0);
      EXPECT_EQ(pFoundAfterFree, nullptr);
  }
  ```

- [ ] **Step 2: Run test to verify it passes under existing compiler configuration**

  Run: `cmake --build build --config Debug` and `build\bin\Debug\dbp_tests.exe --gtest_filter=VarTableTest.DictionaryCaseInsensitiveLookupAndFree`
  Expected: PASS

- [ ] **Step 3: Replace CVarTable static dictionary with std::unordered_map**

  Modify [VarTable.h](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/VarTable.h):
  Remove `db3::TDictionary` references and include `<unordered_map>`.
  ```cpp
  // Replace:
  // #ifdef __AARON_VARTABLEPERF__
  // 		static db3::TDictionary<CVarTable> g_Table;
  // #endif
  // With:
  #ifdef __AARON_VARTABLEPERF__
  		static std::unordered_map<std::string, CVarTable*> g_Table;
  #endif
  ```

  Modify [VarTable.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/VarTable.cpp):
  1. Replace the type definitions and static dictionary instantiation at line 30:
     ```cpp
     #ifdef __AARON_VARTABLEPERF__
     #include <algorithm>
     #include <string>
     #include <unordered_map>

     std::unordered_map<std::string, CVarTable*> CVarTable::g_Table;

     inline std::string to_lower(const std::string& s)
     {
         std::string res = s;
         std::transform(res.begin(), res.end(), res.begin(), ::tolower);
         return res;
     }
     #endif
     ```
  2. Update the constructor at line 84:
     ```cpp
     #ifdef __AARON_VARTABLEPERF__
     	std::string lowerStr = to_lower(pStr);
     	assert_msg(g_Table.find(lowerStr) == g_Table.end() || g_Table[lowerStr] == nullptr, "Variable already exists");
     	g_Table[lowerStr] = this;
     #endif
     ```
  3. Update the destructor at line 95:
     ```cpp
     #ifdef __AARON_VARTABLEPERF__
     	if (m_pVarName)
     	{
     		std::string lowerStr = to_lower(m_pVarName->GetStr());
     		auto it = g_Table.find(lowerStr);
     		if (it != g_Table.end() && it->second == this)
     		{
     			g_Table.erase(it);
     		}
     	}
     #endif
     ```
  4. Update `CVarTable::Free()` at line 107 to erase/clear map entries:
     ```cpp
     void CVarTable::Free(void)
     {
     #ifdef __AARON_VARTABLEPERF__
     	g_Table.clear();
     #endif
     	CVarTable* pCurrent = this;
     	while(pCurrent)
     	{
     		CVarTable* pNext = pCurrent->GetNext();
     		delete pCurrent;
     		pCurrent = pNext;
     	}
     }
     ```
  5. Update `CVarTable::AddVariable` at line 425:
     ```cpp
     #ifdef __AARON_VARTABLEPERF__
     	const char *pIntVarName = MakeIntVarName(pVarScopeStr->GetStr(), pName);
     	std::string lowerIntVarName = to_lower(pIntVarName);
     	assert_msg(g_Table.find(lowerIntVarName) == g_Table.end() || g_Table[lowerIntVarName] == nullptr, "Variable already exists");
     	g_Table[lowerIntVarName] = pNewVar;
     #endif
     ```
  6. Update `CVarTable::FindVariable` at line 445:
     ```cpp
     #ifdef __AARON_VARTABLEPERF__
     	const char *pIntName = MakeIntVarName(pScope, pName);
     	std::string lowerIntName = to_lower(pIntName);
     	auto it = g_Table.find(lowerIntName);
     	if (it == g_Table.end() || !it->second)
     		return nullptr;

     	if (it->second->GetArrFlag()!=dwArrFlag)
     		return nullptr;

     	return it->second;
     #else
     ```

- [ ] **Step 4: Run test to verify it passes**

  Run: `cmake --build build --config Debug` and `build\bin\Debug\dbp_tests.exe --gtest_filter=VarTableTest.*`
  Expected: PASS

- [ ] **Step 5: Commit Task 1**

  Run:
  ```bash
  git add DBProCompiler/DBPCompiler/VarTable.h DBProCompiler/DBPCompiler/VarTable.cpp tests/test_vartable.cpp
  git commit -m "refactor: modernize CVarTable dictionary to std::unordered_map"
  ```

---

### Task 2: Modernize Label Table (`CLabelTable`) Dictionary

**Files:**
- Modify: `DBProCompiler/DBPCompiler/LabelTable.h`
- Modify: `DBProCompiler/DBPCompiler/LabelTable.cpp`
- Modify: `tests/test_structural.cpp`

- [ ] **Step 1: Write a unit test verifying label lookups and Free() cleanups**

  Modify [test_structural.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/tests/test_structural.cpp) to verify `CLabelTable` behavior:
  ```cpp
  #include "LabelTable.h"

  TEST(LabelTableTest, DictionaryCaseInsensitiveLookupAndFree) {
      CLabelTable* pHead = new CLabelTable("*");
      
      bool bRes = pHead->AddLabel("MyLabel", 100, 200, nullptr);
      EXPECT_TRUE(bRes);
      
      CLabelTable* pFound = pHead->FindLabel("mylabel");
      ASSERT_NE(pFound, nullptr);
      EXPECT_STREQ(pFound->GetName()->GetStr(), "MyLabel");
      
      pHead->Free();
      
      CLabelTable* pFoundAfterFree = pHead->FindLabel("mylabel");
      EXPECT_EQ(pFoundAfterFree, nullptr);
  }
  ```

- [ ] **Step 2: Run test to verify it fails/compiles under legacy configuration**

  Run: `cmake --build build --config Debug` and `build\bin\Debug\dbp_tests.exe --gtest_filter=LabelTableTest.*`
  Expected: PASS (on legacy `db3::TDictionary`)

- [ ] **Step 3: Replace CLabelTable static dictionary with std::unordered_map**

  Modify [LabelTable.h](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/LabelTable.h):
  Include `<unordered_map>` and replace the static dictionary declaration:
  ```cpp
  // Replace:
  // #ifdef __AARON_LBLTBLPERF__
  // 		static db3::TDictionary<CLabelTable> g_Table;
  // #endif
  // With:
  #ifdef __AARON_LBLTBLPERF__
  		static std::unordered_map<std::string, CLabelTable*> g_Table;
  #endif
  ```

  Modify [LabelTable.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/LabelTable.cpp):
  1. Replace the type definitions and static dictionary instantiation at line 20:
     ```cpp
     #ifdef __AARON_LBLTBLPERF__
     #include <algorithm>
     #include <string>
     #include <unordered_map>

     std::unordered_map<std::string, CLabelTable*> CLabelTable::g_Table;

     static std::string to_lower(const std::string& s)
     {
         std::string res = s;
         std::transform(res.begin(), res.end(), res.begin(), ::tolower);
         return res;
     }
     #endif
     ```
  2. Update the constructor at line 51:
     ```cpp
     #ifdef __AARON_LBLTBLPERF__
     	std::string lowerStr = to_lower(pStr);
     	assert_msg(g_Table.find(lowerStr) == g_Table.end() || g_Table[lowerStr] == nullptr, "Label already exists");
     	g_Table[lowerStr] = this;
     #endif
     ```
  3. Update the destructor at line 62:
     ```cpp
     #ifdef __AARON_LBLTBLPERF__
     	if (m_pName)
     	{
     		std::string lowerStr = to_lower(m_pName->GetStr());
     		auto it = g_Table.find(lowerStr);
     		if (it != g_Table.end() && it->second == this)
     		{
     			g_Table.erase(it);
     		}
     	}
     #endif
     ```
  4. Update `CLabelTable::Free()` at line 74:
     ```cpp
     void CLabelTable::Free(void)
     {
     #ifdef __AARON_LBLTBLPERF__
     	g_Table.clear();
     #endif
     	CLabelTable* pCurrent = this;
     	while(pCurrent)
     	{
     		CLabelTable* pNext = pCurrent->GetNext();
     		delete pCurrent;
     		pCurrent = pNext;
     	}
     }
     ```
  5. Update `CLabelTable::AddLabel` at line 181:
     ```cpp
     #ifdef __AARON_LBLTBLPERF__
     	std::string lowerName = to_lower(pStr->GetStr());
     	assert_msg(g_Table.find(lowerName) == g_Table.end() || g_Table[lowerName] == nullptr, "Label already exists");
     	g_Table[lowerName] = pNewData;
     #endif
     ```
  6. Update `CLabelTable::FindLabel` at line 199:
     ```cpp
     #ifdef __AARON_LBLTBLPERF__
     	std::string lowerLabelName = to_lower(pLabelName);
     	auto it = g_Table.find(lowerLabelName);
     	if (it == g_Table.end() || !it->second)
     		return nullptr;

     	return it->second;
     #else
     ```

- [ ] **Step 4: Run test to verify it passes**

  Run: `cmake --build build --config Debug` and `build\bin\Debug\dbp_tests.exe --gtest_filter=LabelTableTest.*`
  Expected: PASS

- [ ] **Step 5: Commit Task 2**

  Run:
  ```bash
  git add DBProCompiler/DBPCompiler/LabelTable.h DBProCompiler/DBPCompiler/LabelTable.cpp tests/test_structural.cpp
  git commit -m "refactor: modernize CLabelTable dictionary to std::unordered_map"
  ```

---

### Task 3: Modernize Struct Table (`CStructTable`) Dictionary

**Files:**
- Modify: `DBProCompiler/DBPCompiler/StructTable.h`
- Modify: `DBProCompiler/DBPCompiler/StructTable.cpp`
- Modify: `tests/test_structural.cpp`

- [ ] **Step 1: Write a unit test verifying structure lookups and Free() cleanups**

  Modify [test_structural.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/tests/test_structural.cpp) to verify `CStructTable` behavior:
  ```cpp
  #include "StructTable.h"

  TEST(StructTableTest, DictionaryCaseInsensitiveLookupAndFree) {
      CStructTable* pHead = new CStructTable();
      
      bool bRes = pHead->AddStruct(99, "MyCustomType", 'T', 12);
      EXPECT_TRUE(bRes);
      
      CStructTable* pFound = pHead->GetStruct("mycustomtype");
      ASSERT_NE(pFound, nullptr);
      EXPECT_STREQ(pFound->GetTypeName()->GetStr(), "MyCustomType");
      
      pHead->Free();
      
      CStructTable* pFoundAfterFree = pHead->GetStruct("mycustomtype");
      EXPECT_EQ(pFoundAfterFree, nullptr);
  }
  ```

- [ ] **Step 2: Run test to verify it fails/compiles under legacy configuration**

  Run: `cmake --build build --config Debug` and `build\bin\Debug\dbp_tests.exe --gtest_filter=StructTableTest.*`
  Expected: PASS (on legacy `db3::TDictionary`)

- [ ] **Step 3: Replace CStructTable static dictionary with std::unordered_map**

  Modify [StructTable.h](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/StructTable.h):
  Include `<unordered_map>` and replace the static dictionary declaration:
  ```cpp
  // Replace:
  // #ifdef __AARON_STRUCPERF__
  // 		static db3::TDictionary<CStructTable> g_Table;
  // #endif
  // With:
  #ifdef __AARON_STRUCPERF__
  		static std::unordered_map<std::string, CStructTable*> g_Table;
  #endif
  ```

  Modify [StructTable.cpp](file:///d:/GitHub-repo/Dark-Basic-Pro/DBProCompiler/DBPCompiler/StructTable.cpp):
  1. Replace the type definitions and static dictionary instantiation at line 27:
     ```cpp
     #ifdef __AARON_STRUCPERF__
     #include <algorithm>
     #include <string>
     #include <unordered_map>

     std::unordered_map<std::string, CStructTable*> CStructTable::g_Table;

     static std::string to_lower(const std::string& s)
     {
         std::string res = s;
         std::transform(res.begin(), res.end(), res.begin(), ::tolower);
         return res;
     }
     #endif
     ```
  2. Update the destructor at line 55:
     ```cpp
     #ifdef __AARON_STRUCPERF__
     	if (m_pTypeName)
     	{
     		std::string lowerStr = to_lower(m_pTypeName->GetStr());
     		auto it = g_Table.find(lowerStr);
     		if (it != g_Table.end() && it->second == this)
     		{
     			g_Table.erase(it);
     		}
     	}
     #endif
     ```
  3. Update `CStructTable::Free()` at line 67:
     ```cpp
     void CStructTable::Free(void)
     {
     #ifdef __AARON_STRUCPERF__
     	g_Table.clear();
     #endif
     	CStructTable* pCurrent = this;
     	while(pCurrent)
     	{
     		CStructTable* pNext = pCurrent->GetNext();
     		delete pCurrent;
     		pCurrent = pNext;
     	}
     }
     ```
  4. Update `CStructTable::SetStruct` at line 125:
     ```cpp
     #ifdef __AARON_STRUCPERF__
     	std::string lowerName = to_lower(pStructName);
     	auto it = g_Table.find(lowerName);
     	assert_msg(it != g_Table.end(), "g_Table.Lookup() failed!");
     	assert_msg(!it->second, "Struct already exists");
     	g_Table[lowerName] = this;
     #endif
     ```
  5. Update `CStructTable::AddStruct` at line 152:
     ```cpp
     #ifdef __AARON_STRUCPERF__
     	std::string lowerName = to_lower(pStructName);
     	auto it = g_Table.find(lowerName);
     	assert_msg(it != g_Table.end(), "g_Table.Lookup() failed!");
     	assert_msg(!it->second, "Struct already exists");
     	g_Table[lowerName] = pNewType;
     #endif
     ```
  6. Update `CStructTable::GetStruct` at line 400:
     ```cpp
     #ifdef __AARON_STRUCPERF__
     	std::string lowerTypename = to_lower(pTypename);
     	auto it = g_Table.find(lowerTypename);
     	if (it == g_Table.end() || !it->second)
     		return nullptr;
     	return it->second;
     #else
     ```

- [ ] **Step 4: Run test to verify it passes**

  Run: `cmake --build build --config Debug` and `build\bin\Debug\dbp_tests.exe --gtest_filter=StructTableTest.*`
  Expected: PASS

- [ ] **Step 5: Commit Task 3**

  Run:
  ```bash
  git add DBProCompiler/DBPCompiler/StructTable.h DBProCompiler/DBPCompiler/StructTable.cpp tests/test_structural.cpp
  git commit -m "refactor: modernize CStructTable dictionary to std::unordered_map"
  ```
