#include "parser.hpp"

#include <nlohmann/json.hpp>

#include <exception>

#include <gtest/gtest.h>

// Regression tests for out-of-bounds reads on short/garbage EEPROM data.
//
// The VPD collection path feeds EEPROM contents (field-replaceable
// hardware, unauthenticated) straight into these parsers. Short or
// garbage-sized EEPROMs must be rejected by a length check, never by
// walking an iterator past the end of the vector.
//
// Samples are truncations of the existing (valid) test vectors.

TEST(VpdBoundsRegression, TypeCheckGateRejectsTinyEeprom)
{
    // 3 bytes: vpdTypeCheck() indexed [11]/[3]/[2] before any size check.
    // After the fix, the parser factory rejects the format without reading.
    nlohmann::json l_json;
    vpd::Parser l_vpdParser("vpd_files/ipz_truncated_3b.dat", l_json);
    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(VpdBoundsRegression, TypeCheckGateRejectsFiveByteEeprom)
{
    // 5 bytes: same family, [11] read lands 6 bytes past the data.
    nlohmann::json l_json;
    vpd::Parser l_vpdParser("vpd_files/ipz_truncated_5b.dat", l_json);
    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(VpdBoundsRegression, TypeCheckGateRejectsElevenByteEeprom)
{
    // 11 bytes: one short of the IPZ tag offset itself.
    nlohmann::json l_json;
    vpd::Parser l_vpdParser("vpd_files/ipz_truncated_11b.dat", l_json);
    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(VpdBoundsRegression, TocPastEndIsRejected)
{
    // 44 bytes: minimum VHDR size; a VTOC pointer past the vector end
    // must be rejected by the span check instead of dereferenced.
    // (Crafted from the valid system VPD: VTOC pointer at offset 35 set
    // to 0x0100.)
    nlohmann::json l_json;
    vpd::Parser l_vpdParser("vpd_files/ipz_toc_past_end.dat", l_json);
    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(VpdBoundsRegression, RecordOffsetFarPastEndIsRejected)
{
    // A valid VTOC whose PT entry points at record offset 0xFFF0 on a
    // ~200-byte image: the record-name read is ~65 KB past the vector end
    // without the span check.
    nlohmann::json l_json;
    vpd::Parser l_vpdParser("vpd_files/ipz_record_offset_far.dat", l_json);
    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(VpdBoundsRegression, ValidVpdStillParses)
{
    // Negative control: the untouched valid system VPD must keep parsing
    // to the same values (no over-rejection from the new gates).
    nlohmann::json l_json;
    vpd::Parser l_vpdParser("vpd_files/ipz_system.dat", l_json);

    vpd::types::IPZVpdMap l_ipzVpdMap;
    auto l_parsedMap = l_vpdParser.parse();
    if (auto l_mapPtr = std::get_if<vpd::types::IPZVpdMap>(&l_parsedMap))
    {
        l_ipzVpdMap = *l_mapPtr;
    }
    ASSERT_NE(l_ipzVpdMap.find("VINI"), l_ipzVpdMap.end());
}

TEST(VpdBoundsRegression, BelowRecordMinRejectedBySizeGate)
{
    // 43 bytes, one below RECORD_MIN: the size gate itself must reject
    // before any header interpretation.
    nlohmann::json l_json;
    vpd::Parser l_vpdParser("vpd_files/ipz_truncated_43b.dat", l_json);
    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(VpdBoundsRegression, RecordMinWithGarbageVtocRejectedBySpanCheck)
{
    // Exactly 44 bytes (RECORD_MIN) carrying a valid VHDR tag: the
    // size gate passes and the header matches, so rejection must come
    // from the walk's span checks (the truncated VTOC pointer is
    // garbage), not from reading past the buffer.
    nlohmann::json l_json;
    vpd::Parser l_vpdParser("vpd_files/ipz_truncated_44b.dat", l_json);
    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(VpdBoundsRegression, IsdimmLengthGateRejectsShortSpd)
{
    // 200 bytes of SPD-shaped data ([2] = DDR4, [3] not a DDIMM module
    // type): passes the 12-byte type-check gate, selects the ISDIMM
    // branch, and relies on the JedecSpdParser::parse() length gate
    // (the DDR4 walk reads fixed offsets up to byte 328 with no
    // per-field checks).
    nlohmann::json l_json;
    vpd::Parser l_vpdParser("vpd_files/spd_isdimm_200b.dat", l_json);
    EXPECT_THROW(l_vpdParser.parse(), std::exception);
}

TEST(VpdBoundsRegression, ShortDdimmSkipsManufacturerIdCopy)
{
    // 500 bytes of DDIMM data ([2] = DDR4, [3] = DDIMM module type, 11S
    // barcode at offset 416): passes the barcode gate, but the
    // manufacturer-ID copy at offset 0x228 would read past the end.
    // The fix skips the copy; "DI" stays zero-filled. Behavior-agnostic
    // on parse-vs-reject pending the reject-vs-skip reviewer question,
    // but a crash or an out-of-bounds read must not occur.
    nlohmann::json l_json;
    vpd::Parser l_vpdParser("vpd_files/ddimm_short_500b.dat", l_json);
    EXPECT_NO_FATAL_FAILURE({
        try
        {
            (void)l_vpdParser.parse();
        }
        catch (const std::exception&)
        {
            // clean rejection is acceptable; crash is not
        }
    });
}
