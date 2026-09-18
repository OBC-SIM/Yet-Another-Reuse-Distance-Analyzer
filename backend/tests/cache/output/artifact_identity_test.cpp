#include "artifact_test_support.hpp"
#include "temporary_input.hpp"

namespace
{
using namespace yarda;
using namespace yarda::test::artifact;

TEST(ArtifactIdentityTest, HashesEmptyFileWithStandardVector)
{
  TemporaryInput input;
  input.write("");
  EXPECT_EQ(sha256_file_bytes(input.path),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(ArtifactIdentityTest, HashesAbcWithStandardVector)
{
  TemporaryInput input;
  input.write("abc");
  EXPECT_EQ(sha256_file_bytes(input.path),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(ArtifactIdentityTest, PreservesBinaryNulAndNonUtf8Bytes)
{
  TemporaryInput input;
  input.write(std::string("a\0b\xff", 4));
  EXPECT_EQ(sha256_file_bytes(input.path),
            "a37cc3026aae4d519e0b19c298fa913b4dccfdf0658cbccbb7deaa0226d5acdb");
}

TEST(ArtifactIdentityTest, HashesAcrossMultipleReadBuffers)
{
  TemporaryInput input;
  input.write(std::string(131073, 'x'));
  EXPECT_EQ(sha256_file_bytes(input.path),
            "0c5c5c759aa8164f9fb53c471ff060903c99edcb520fb7d9cb4bf7a45755f1c2");
}

TEST(ArtifactIdentityTest, RawWhitespaceChangesConfigIdentity)
{
  TemporaryInput input;
  input.write("schema_version: 1\n");
  const auto first = sha256_file_bytes(input.path);
  input.write("schema_version:  1\n");
  EXPECT_NE(first, sha256_file_bytes(input.path));
}

TEST(ArtifactIdentityTest, MissingFileIsAnExplicitReadError)
{
  TemporaryInput input;
  EXPECT_THROW(sha256_file_bytes(input.path), std::runtime_error);
}

TEST(ArtifactIdentityTest, UnreadableFileContentsAreAnExplicitReadError)
{
  TemporaryInput input;
  EXPECT_THROW(sha256_file_bytes(input.directory.string()), std::runtime_error);
}

TEST(ArtifactIdentityTest, MatchesIndependentCanonicalPreimageDigest)
{
  EXPECT_EQ(hierarchy_analysis_id(identity()), kAnalysisId);
}

TEST(ArtifactIdentityTest, CanonicalizesNestedObjectConstructionOrder)
{
  auto left = identity();
  auto right = identity();
  left.semantic_analysis_options = {{"z", {{"b", true}, {"a", -2}}},
                                    {"a", "한글\n\""}};
  right.semantic_analysis_options = {{"a", "한글\n\""},
                                     {"z", {{"a", -2}, {"b", true}}}};
  EXPECT_EQ(hierarchy_analysis_id(left), hierarchy_analysis_id(right));
}

TEST(ArtifactIdentityTest, PreservesSemanticArrayOrder)
{
  auto value = identity();
  value.semantic_analysis_options = {{"order", {1, 2}}};
  const auto before = hierarchy_analysis_id(value);
  value.semantic_analysis_options["order"] = {2, 1};
  EXPECT_NE(before, hierarchy_analysis_id(value));
}

TEST(ArtifactIdentityTest, ToolVersionAndEachRawHashChangeAnalysisId)
{
  for (auto field :
       {&AnalysisIdentityInput::tool_version,
        &AnalysisIdentityInput::map_sha256, &AnalysisIdentityInput::elf_sha256,
        &AnalysisIdentityInput::cache_config_sha256})
  {
    auto value = identity();
    (value.*field)[0] = 'd';
    EXPECT_NE(hierarchy_analysis_id(value), kAnalysisId);
  }
}

TEST(ArtifactIdentityTest, EffectiveSemanticOptionChangesAnalysisId)
{
  auto value = identity();
  value.semantic_analysis_options = {{"selection", "alpha"}};
  const auto before = hierarchy_analysis_id(value);
  value.semantic_analysis_options["selection"] = "beta";
  EXPECT_NE(before, hierarchy_analysis_id(value));
}

TEST(ArtifactIdentityTest, RejectsIncompleteVersionAndNonzeroCore)
{
  auto value = identity();
  value.tool_version.clear();
  EXPECT_THROW(hierarchy_analysis_id(value), std::invalid_argument);
  value = identity();
  value.analysis_core_id = 1;
  EXPECT_THROW(hierarchy_analysis_id(value), std::invalid_argument);
}

TEST(ArtifactIdentityTest, RejectsNoncanonicalHashSyntax)
{
  for (const auto & hash : {std::string{}, std::string(63, 'a'),
                            std::string(64, 'A'), std::string(64, 'g')})
  {
    auto value = identity();
    value.map_sha256 = hash;
    EXPECT_THROW(hierarchy_analysis_id(value), std::invalid_argument);
  }
}

TEST(ArtifactIdentityTest, RejectsUnsupportedCanonicalValueTypes)
{
  for (const auto & bad : {nlohmann::json(nullptr), nlohmann::json(0.5),
                           nlohmann::json::binary({1, 2})})
  {
    auto value = identity();
    value.semantic_analysis_options = {{"nested", {{"value", bad}}}};
    EXPECT_THROW(hierarchy_analysis_id(value), std::invalid_argument);
  }
  auto value = identity();
  value.semantic_analysis_options = nlohmann::json::array();
  EXPECT_THROW(hierarchy_analysis_id(value), std::invalid_argument);
}

} // namespace
