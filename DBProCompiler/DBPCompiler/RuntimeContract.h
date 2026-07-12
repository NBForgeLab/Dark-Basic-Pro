#pragma once

#include <cstdint>
#include <set>

enum class RuntimeCapability {
    CoreBootstrapV1,
    CoreDataStatementsV1,
    CoreStructurePatternsV1,
    CoreRuntimeErrorsV1
};

using RuntimeCapabilities = std::set<RuntimeCapability>;
using ProgramRuntimeRequirements = RuntimeCapabilities;

inline RuntimeCapabilities MissingCapabilities(
    const RuntimeCapabilities& available,
    const ProgramRuntimeRequirements& required) {
    RuntimeCapabilities missing;
    for (const auto capability : required) {
        if (available.count(capability) == 0) {
            missing.insert(capability);
        }
    }
    return missing;
}

inline ProgramRuntimeRequirements DeriveProgramRuntimeRequirements(
    const std::uint32_t structurePatternCount) {
    ProgramRuntimeRequirements requirements{
        RuntimeCapability::CoreBootstrapV1,
        RuntimeCapability::CoreDataStatementsV1,
        RuntimeCapability::CoreRuntimeErrorsV1};
    if (structurePatternCount > 0) {
        requirements.insert(RuntimeCapability::CoreStructurePatternsV1);
    }
    return requirements;
}
