#include "mp4Processor.h"
#include <Ap4.h>
#include <Ap4File.h>
#include <Ap4MovieFragment.h>
#include <vector>
#include <array>
#include <map>
#include <Ap4StreamCipher.h>
#include <string>
#include <iostream>
#include <algorithm>
#include "routeObject.h"
#ifdef USE_CAS_DECRYPTION
#include "httplib.h"
#endif

namespace {

std::vector<uint8_t> hexstr_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.size(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

std::array<uint8_t, 16> toArray16(const std::vector<uint8_t>& vec) {
    if (vec.size() != 16) {
        throw std::runtime_error("vector size must be 16");
    }

    std::array<uint8_t, 16> arr;
    std::copy(vec.begin(), vec.end(), arr.begin());
    return arr;
}

}
bool MP4ConfigParser::parse(const std::vector<uint8_t>& input, MP4Config& config)
{
    AP4_DataBuffer buffer;
    buffer.SetData(static_cast<const AP4_UI08*>(input.data()), static_cast<AP4_Size>(input.size()));

    AP4_MemoryByteStream* stream = new AP4_MemoryByteStream(buffer);
    AP4_AtomFactory atom_factory;
    AP4_File* file = new AP4_File(*stream, atom_factory, false);
    AP4_List<AP4_Atom>::Item* atom = file->GetTopLevelAtoms().FirstItem();
    if (!atom) {
        return false;
    }

    while (atom) {
        if (atom->GetData()->GetType() == AP4_ATOM_TYPE_MOOV) {
            AP4_ContainerAtom* moov = AP4_DYNAMIC_CAST(AP4_ContainerAtom, atom->GetData());

            AP4_Atom* mdhdFind = moov->FindChild("trak/mdia/mdhd");
            AP4_MdhdAtom* mdhd = AP4_DYNAMIC_CAST(AP4_MdhdAtom, mdhdFind);
            config.timescale = mdhd->GetTimeScale();


            AP4_Atom* hvccFind = moov->FindChild("trak/mdia/minf/stbl/stsd/hev1/hvcC");
            if (hvccFind == nullptr) {
                hvccFind = moov->FindChild("trak/mdia/minf/stbl/stsd/encv/hvcC");
            }
            if (hvccFind) {
                AP4_HvccAtom* hvcc = AP4_DYNAMIC_CAST(AP4_HvccAtom, hvccFind);
                uint32_t len = hvcc->GetNaluLengthSize();

                std::vector<uint8_t> hevcConfigNalUnits;

                if (hvcc->GetSequences().ItemCount() > 0x20) {
                    return false;
                }

                for (uint32_t i = 0; i < hvcc->GetSequences().ItemCount(); i++) {
                    for (uint32_t i2 = 0; i2 < hvcc->GetSequences()[i2].m_Nalus.ItemCount(); i2++) {
                        if (hvcc->GetSequences()[i2].m_Nalus.ItemCount() > 0x20) {
                            return false;
                        }

                        hevcConfigNalUnits.insert(hevcConfigNalUnits.end(), { 0x00, 0x00, 0x00, 0x01 });
                        hevcConfigNalUnits.insert(hevcConfigNalUnits.end(), hvcc->GetSequences()[i].m_Nalus[i2].GetData(), hvcc->GetSequences()[i].m_Nalus[i2].GetData() + hvcc->GetSequences()[i].m_Nalus[i2].GetDataSize());
                    }
                }

                config.configNalUnits = hevcConfigNalUnits;
            }

        }
        atom = atom->GetNext();
    }

    delete file;
    stream->Release();

    return true;
}

void MP4Processor::ProcessTrun(AP4_TrunAtom* trun) {
    for (uint32_t i = 0; i < trun->GetEntries().ItemCount(); i++) {
        if (trun->GetEntries()[i].sample_size == 0) {
            break;
        }
        vecSampleSize.push_back(trun->GetEntries()[i].sample_size);
        vecSampleCompositionTimeOffset.push_back(trun->GetEntries()[i].sample_composition_time_offset);
    }
}

void MP4Processor::ProcessTfdt(AP4_TfdtAtom* tfdt) {
    baseDts = tfdt->GetBaseMediaDecodeTime();
}

bool MP4Processor::ProcessTfhd(AP4_TfhdAtom* tfhd) {
    baseSampleDuration = tfhd->GetDefaultSampleDuration();
    return false;
}

void MP4Processor::clear()
{
    vecIv.clear();
    vecSampleSize.clear();
    vecSampleCompositionTimeOffset.clear();
    packets.clear();
}

void MP4Processor::ProcessSenc(AP4_Atom* trun) {
    AP4_DataBuffer buffer;
    AP4_MemoryByteStream* stream = new AP4_MemoryByteStream(buffer);

    trun->Write(*stream);

    stream->Seek(12);

    AP4_UI32 sample_count = 0;
    stream->ReadUI32(sample_count);

    for (AP4_UI32 i = 0; i < sample_count; i++) {
        std::array<uint8_t, 16> iv;
        stream->Read(iv.data(), 16);
        vecIv.push_back(iv);
    }

    stream->Release();
}

bool MP4Processor::ProcessPssh(AP4_Atom* trun) {
    AP4_DataBuffer buffer;
    AP4_MemoryByteStream* stream = new AP4_MemoryByteStream(buffer);

    trun->Write(*stream);

    stream->Seek(0x20);

    std::array<uint8_t, 16> kid;
    stream->Read(kid.data(), 16);

    currentKid = kid;

    auto it = std::find_if(keyCache.begin(), keyCache.end(),
        [&](const auto& pair) {
            return pair.first == kid;
        });


#ifdef USE_CAS_DECRYPTION
    if (it == keyCache.end()) {

        AP4_UI32 ecmSize = 0;
        stream->ReadUI32(ecmSize);

        std::vector<uint8_t> ecm(ecmSize);
        stream->Read(ecm.data(), ecmSize);

        httplib::Client cli("http://192.168.0.12:8084");

        std::vector<uint8_t> data;
        data.insert(data.end(), kid.begin(), kid.end());
        data.insert(data.end(), ecm.begin(), ecm.end());

        std::string body(data.begin(), data.end());
        auto res = cli.Post("/v1/cas/ecm", body, "application/octet-stream");

        if (res && res->status == 200) {
            std::istringstream iss(res.value().body);
            std::string line1, line2;
            std::getline(iss, line1);
            std::getline(iss, line2);

            std::vector<uint8_t> kid = hexstr_to_bytes(line1);
            std::vector<uint8_t> key = hexstr_to_bytes(line2);

            keyCache.emplace_back(toArray16(kid), toArray16(key));

            if (keyCache.size() > 100) {
                keyCache.pop_front();
            }

        }
        else {
            return false;
        }
    }
#endif

    stream->Release();
    return true;
}

bool MP4Processor::ProcessMdat(AP4_Atom* trun, std::vector<uint8_t>& outputMp4) {
    AP4_DataBuffer buffer;
    AP4_MemoryByteStream* stream = new AP4_MemoryByteStream(buffer);

    trun->Write(*stream);

    std::vector<uint8_t> decryptedMdat;

    bool hasPrefix = false;
    stream->Seek(0x8);

    int size = 0;
    uint64_t dts = baseDts;
    for (int i = 0; i < vecSampleSize.size(); i++) {
        std::vector<uint8_t> segment(vecSampleSize[i]);
        stream->Read(segment.data(), vecSampleSize[i]);

#ifdef USE_CAS_DECRYPTION
        std::vector<uint8_t> decrypted(vecSampleSize[i]);
        if (vecIv.size() < i + 1) {
            decryptedMdat.insert(decryptedMdat.end(), segment.begin(), segment.end());

            StreamPacket packet;
            packet.data = segment;
            dts += baseSampleDuration;
            packet.dts = dts;
            packet.pts = dts + vecSampleCompositionTimeOffset[i];
            packets.push_back(packet);
        }
        else {
            AP4_BlockCipher* block_cipher = NULL;
            AP4_BlockCipher::CtrParams ctr_params;
            ctr_params.counter_size = 8;

            auto it = std::find_if(keyCache.begin(), keyCache.end(),
                [&](const auto& pair) {
                    return pair.first == currentKid;
                }
            );

            if (it == keyCache.end()) {
                stream->Release();
                return false;
            }

            auto block_cipher_factory = &AP4_DefaultBlockCipherFactory::Instance;
            AP4_Result result = block_cipher_factory->CreateCipher(AP4_BlockCipher::AES_128,
                AP4_BlockCipher::DECRYPT,
                AP4_BlockCipher::CTR,
                &ctr_params,
                it->second.data(),
                16,
                block_cipher);

            AP4_CtrStreamCipher* stream_cipher = new AP4_CtrStreamCipher(block_cipher, 16);
            stream_cipher->SetIV(vecIv[i].data());
            stream_cipher->ProcessBuffer(segment.data(), segment.size(), decrypted.data());
            delete stream_cipher;

            decryptedMdat.insert(decryptedMdat.end(), decrypted.begin(), decrypted.end());

            StreamPacket packet;
            packet.data = std::move(decrypted);
            dts += baseSampleDuration;
            packet.dts = dts;
            packet.pts = dts + vecSampleCompositionTimeOffset[i];
            packets.push_back(packet);
        }
#else
        decryptedMdat.insert(decryptedMdat.end(), segment.begin(), segment.end());

        StreamPacket packet;
        packet.data = segment;
        dts += baseSampleDuration;
        packet.dts = dts;
        packet.pts = dts + vecSampleCompositionTimeOffset[i];
        packets.push_back(packet);
#endif
    }

    {
        AP4_UnknownAtom newMdat(AP4_ATOM_TYPE_MDAT, decryptedMdat.data(), static_cast<AP4_Size>(decryptedMdat.size()));
        AP4_MemoryByteStream* stream2 = new AP4_MemoryByteStream(buffer);
        newMdat.Write(*stream2);
        outputMp4.insert(outputMp4.end(), buffer.GetData(), buffer.GetData() + buffer.GetDataSize());

        stream2->Release();
    }

    stream->Release();
    return true;
}

bool MP4Processor::ProcessMoof(AP4_ContainerAtom* moof) {
    if (!moof) {
        return true;
    }

    AP4_List<AP4_Atom>::Item* item = moof->GetChildren().FirstItem();
    while (item) {
        AP4_Atom* atom = item->GetData();
        if (atom->GetType() == AP4_ATOM_TYPE_TRAF) {
            AP4_ContainerAtom* traf = AP4_DYNAMIC_CAST(AP4_ContainerAtom, atom);
            if (!traf) continue;

            bool trunProcessed = false;
            bool sencProcessed = false;
            bool tfdtProcessed = false;
            bool tfhdProcessed = false;
            AP4_List<AP4_Atom>::Item* traf_item = traf->GetChildren().FirstItem();
            while (traf_item) {
                AP4_Atom* traf_atom = traf_item->GetData();
                if (traf_atom->GetType() == AP4_ATOM_TYPE_TRUN && !trunProcessed) {
                    AP4_TrunAtom* trun = AP4_DYNAMIC_CAST(AP4_TrunAtom, traf_atom);
                    ProcessTrun(trun);
                    trunProcessed = true;
                }
                else if (traf_atom->GetType() == AP4_ATOM_TYPE_SENC && !sencProcessed) {
                    ProcessSenc(traf_atom);
                    sencProcessed = true;
                }
                else if (traf_atom->GetType() == AP4_ATOM_TYPE_TFDT && !tfdtProcessed) {
                    AP4_TfdtAtom* tfdt = AP4_DYNAMIC_CAST(AP4_TfdtAtom, traf_atom);
                    ProcessTfdt(tfdt);
                    tfdtProcessed = true;
                }
                else if (traf_atom->GetType() == AP4_ATOM_TYPE_TFHD && !tfhdProcessed) {
                    AP4_TfhdAtom* tfhd = AP4_DYNAMIC_CAST(AP4_TfhdAtom, traf_atom);
                    ProcessTfhd(tfhd);
                    tfhdProcessed = true;
                }

                
                traf_item = traf_item->GetNext();
            }
        }
        else if (atom->GetType() == AP4_ATOM_TYPE_PSSH) {
            if (!ProcessPssh(atom)) {
                return false;
            }
        }
        item = item->GetNext();
    }

    return true;
}

static void renameAudioSampleEntry(std::vector<uint8_t>& data) {
    const std::vector<uint8_t> target = { 'e', 'n', 'c', 'a', 0x00, 0x00, 0x00, 0x00};
    const std::vector<uint8_t> replacement = { 'm', 'h', 'm', '1', 0x00, 0x00, 0x00, 0x00};

    auto it = std::search(data.begin(), data.end(), target.begin(), target.end());
    while (it != data.end()) {
        std::copy(replacement.begin(), replacement.end(), it);
        it = std::search(it + replacement.size(), data.end(), target.begin(), target.end());
    }
}

bool MP4Processor::process(const std::vector<uint8_t>& data, std::vector<StreamPacket>& packets, std::vector<uint8_t>& decryptedMP4, uint64_t& baseDts)
{
    clear();

    AP4_DataBuffer buffer;
    buffer.SetData((AP4_UI08*)data.data(), static_cast<AP4_Size>(data.size()));

    AP4_MemoryByteStream* stream = new AP4_MemoryByteStream(buffer);
    AP4_AtomFactory atom_factory;
    AP4_File* file = new AP4_File(*stream, atom_factory, false);
    AP4_List<AP4_Atom>::Item* atom = file->GetTopLevelAtoms().FirstItem();
    if (!atom) {
        return false;
    }

    while (atom) {
        if (atom->GetData()->GetType() == AP4_ATOM_TYPE_MOOF) {
            if (!ProcessMoof(AP4_DYNAMIC_CAST(AP4_ContainerAtom, atom->GetData()))) {
                return false;
            }
        }
        else if (atom->GetData()->GetType() == AP4_ATOM_TYPE_MDAT) {
            ProcessMdat(atom->GetData(), decryptedMP4);

            atom = atom->GetNext();
            continue;
        }

        AP4_DataBuffer buffer;
        AP4_MemoryByteStream* stream = new AP4_MemoryByteStream(buffer);
        atom->GetData()->Write(*stream);
        decryptedMP4.insert(decryptedMP4.end(), buffer.GetData(), buffer.GetData() + buffer.GetDataSize());
        stream->Release();

        atom = atom->GetNext();
    }

    renameAudioSampleEntry(decryptedMP4);

    packets = std::move(this->packets);
    baseDts = this->baseDts;

    delete file;
    stream->Release();
    return true;
}