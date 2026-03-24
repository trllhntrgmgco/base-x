/**
 * ThirdParty/BaseX/ — Adapter header
 *
 * This directory holds the ORIGINAL Base-X Engine headers, copied verbatim
 * from include/BaseX/ in the Base-X Game Engine repository.
 *
 * Files to copy here:
 *   - DRO.h               (Dynamic Radix Optimization)
 *   - WorldPromptEngine.h  (NLP → world generation pipeline)
 *
 * These headers are pure C++ with no rendering or platform dependency.
 * The UE bridge layer in Source/BaseXWorldGen/ wraps them in UCLASS/USTRUCT.
 *
 * IMPORTANT: Keep these in sync with the Base-X repo originals.
 * The Build.cs adds this directory to PublicIncludePaths so the UE module
 * can #include <BaseX/DRO.h> and <BaseX/WorldPromptEngine.h> unchanged.
 */
