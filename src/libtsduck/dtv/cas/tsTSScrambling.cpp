//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2025, Thierry Lelegard
// BSD-2-Clause license, see LICENSE.txt file or https://tsduck.io/license
//
//----------------------------------------------------------------------------

#include "tsTSScrambling.h"
#include "tsArgs.h"


//----------------------------------------------------------------------------
// Constructors.
//----------------------------------------------------------------------------

ts::TSScrambling::TSScrambling(Report& report, uint8_t scrambling) :
    _report(report),
    _scrambling_type(scrambling),
    _next_cw(_cw_list.end())

{
    setScramblingType(scrambling);
}

ts::TSScrambling::TSScrambling(const TSScrambling& other) :
    _report(other._report),
    _scrambling_type(other._scrambling_type),
    _explicit_type(other._explicit_type),
    _cw_list(other._cw_list),
    _next_cw(_cw_list.end())
{
    setScramblingType(_scrambling_type);
    _dvbcsa[0].setEntropyMode(other._dvbcsa[0].entropyMode());
    _dvbcsa[1].setEntropyMode(other._dvbcsa[1].entropyMode());
    _aescbc[0].setIV(other._aescbc[0].currentIV());
    _aescbc[1].setIV(other._aescbc[1].currentIV());
    _aesctr[0].setIV(other._aesctr[0].currentIV());
    _aesctr[1].setIV(other._aesctr[1].currentIV());
    _sm4cbc[0].setIV(other._sm4cbc[0].currentIV());
    _sm4cbc[1].setIV(other._sm4cbc[1].currentIV());
}


//----------------------------------------------------------------------------
// Force the usage of a specific algorithm.
//----------------------------------------------------------------------------

bool ts::TSScrambling::setScramblingType(uint8_t scrambling, bool overrideExplicit)
{
    if (overrideExplicit || !_explicit_type) {

        // Select the right pair of scramblers.
        switch (scrambling) {
            case SCRAMBLING_DVB_CSA1:
            case SCRAMBLING_DVB_CSA2:
                _scrambler[0] = &_dvbcsa[0];
                _scrambler[1] = &_dvbcsa[1];
                break;
            case SCRAMBLING_DVB_CISSA1:
                _scrambler[0] = &_dvbcissa[0];
                _scrambler[1] = &_dvbcissa[1];
                break;
            case SCRAMBLING_ATIS_IIF_IDSA:
                _scrambler[0] = &_idsa[0];
                _scrambler[1] = &_idsa[1];
                break;
            case SCRAMBLING_DUCK_AES_CBC:
                _scrambler[0] = &_aescbc[0];
                _scrambler[1] = &_aescbc[1];
                break;
            case SCRAMBLING_DUCK_AES_CTR:
                _scrambler[0] = &_aesctr[0];
                _scrambler[1] = &_aesctr[1];
                break;
            case SCRAMBLING_DUCK_SM4_ECB:
                _scrambler[0] = &_sm4ecb[0];
                _scrambler[1] = &_sm4ecb[1];
                break;
            case SCRAMBLING_DUCK_SM4_CBC:
                _scrambler[0] = &_sm4cbc[0];
                _scrambler[1] = &_sm4cbc[1];
                break;
            default:
                // Fallback to DVB-CSA2 if no scrambler was previously defined.
                if (_scrambler[0] == nullptr || _scrambler[1] == nullptr) {
                    _scrambling_type = SCRAMBLING_DVB_CSA2;
                    _scrambler[0] = &_dvbcsa[0];
                    _scrambler[1] = &_dvbcsa[1];
                }
                return false;
        }

        // Set scrambling type.
        if (_scrambling_type != scrambling) {
            if (_report.debug()) {
                _report.debug(u"switching scrambling type from %s to %s", NameFromSection(u"dtv", u"ScramblingMode", _scrambling_type), NameFromSection(u"dtv", u"ScramblingMode", scrambling));
            }
            _scrambling_type = scrambling;
        }
    }

    // Make sure the current scramblers notify alerts to this object.
    _scrambler[0]->setAlertHandler(this);
    _scrambler[1]->setAlertHandler(this);
    _scrambler[0]->setCipherId(0);
    _scrambler[1]->setCipherId(1);
    return true;
}

void ts::TSScrambling::setEntropyMode(DVBCSA2::EntropyMode mode)
{
    _dvbcsa[0].setEntropyMode(mode);
    _dvbcsa[1].setEntropyMode(mode);
}

ts::DVBCSA2::EntropyMode ts::TSScrambling::entropyMode() const
{
    return (_scrambling_type == SCRAMBLING_DVB_CSA1 || _scrambling_type == SCRAMBLING_DVB_CSA2) ?
            _dvbcsa[0].entropyMode() : DVBCSA2::FULL_CW;
}


//----------------------------------------------------------------------------
// Define command line options in an Args.
//----------------------------------------------------------------------------

void ts::TSScrambling::defineArgs(Args &args)
{
    args.option(u"sm4-cbc");
    args.help(u"sm4-cbc",
              u"Use SM4-CBC scrambling instead of DVB-CSA2 (the default). "
              u"The control words are 16-byte long instead of 8-byte. "
              u"The residue is left clear. "
              u"Specify a fixed initialization vector using the --iv option.\n\n"
              u"Note that this is a non-standard TS scrambling mode. "
              u"The TSDuck scrambler automatically sets the scrambling_descriptor with "
              u"user-defined value " + UString::Hexa(uint8_t(SCRAMBLING_DUCK_SM4_CBC)) + u".");

    args.option(u"sm4-ecb");
    args.help(u"sm4-ecb",
              u"Use SM4-ECB scrambling instead of DVB-CSA2 (the default). "
              u"The control words are 16-byte long instead of 8-byte. "
              u"The residue is left clear. "
              u"Specify a fixed initialization vector using the --iv option.\n\n"
              u"Note that this is a non-standard TS scrambling mode. "
              u"The TSDuck scrambler automatically sets the scrambling_descriptor with "
              u"user-defined value " + UString::Hexa(uint8_t(SCRAMBLING_DUCK_SM4_ECB)) + u".");

    args.option(u"aes-cbc");
    args.help(u"aes-cbc",
              u"Use AES-CBC scrambling instead of DVB-CSA2 (the default). "
              u"The control words are 16-byte long instead of 8-byte. "
              u"The residue is left clear. "
              u"Specify a fixed initialization vector using the --iv option.\n\n"
              u"Note that this is a non-standard TS scrambling mode. "
              u"The only standard AES-based scrambling modes are ATIS-IDSA and DVB-CISSA "
              u"(DVB-CISSA is the same as AES-CBC with a DVB-defined IV). "
              u"The TSDuck scrambler automatically sets the scrambling_descriptor with "
              u"user-defined value " + UString::Hexa(uint8_t(SCRAMBLING_DUCK_AES_CBC)) + u".");

    args.option(u"aes-ctr");
    args.help(u"aes-ctr",
              u"Use AES-CTR scrambling instead of DVB-CSA2 (the default). "
              u"The control words are 16-byte long instead of 8-byte. "
              u"The residue is included in the scrambling. "
              u"Specify a fixed initialization vector using the --iv option. "
              u"See the option --ctr-counter-bits for the size of the counter part in the IV.\n\n"
              u"Note that this is a non-standard TS scrambling mode. "
              u"The only standard AES-based scrambling modes are ATIS-IDSA and DVB-CISSA. "
              u"The TSDuck scrambler automatically sets the scrambling_descriptor with "
              u"user-defined value " + UString::Hexa(uint8_t(SCRAMBLING_DUCK_AES_CTR)) + u".");

    args.option(u"atis-idsa");
    args.help(u"atis-idsa",
              u"Use ATIS-IDSA scrambling (ATIS-0800006) instead of DVB-CSA2 (the "
              u"default). The control words are 16-byte long instead of 8-byte.");

    args.option(u"iv", 0, Args::HEXADATA, 0, Args::UNLIMITED_COUNT, AES128::BLOCK_SIZE, AES128::BLOCK_SIZE);
    args.help(u"iv",
              u"With --aes-cbc or --aes-ctr, specifies a fixed initialization vector for all TS packets. "
              u"The value must be a string of 32 hexadecimal digits. "
              u"The default IV is all zeroes.");

    args.option(u"ctr-counter-bits", 0, Args::UNSIGNED);
    args.help(u"ctr-counter-bits",