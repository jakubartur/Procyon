// Copyright (c) 2017 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util.h>
#include <consensus/params.h>
#include <script/ismine.h>
#include <tinyformat.h>
#include "assetdb.h"
#include "assets.h"
#include "validation.h"

static const char ASSET_FLAG = 'A';
static const char ASSET_ADDRESS_QUANTITY_FLAG = 'B';
static const char MY_ASSET_FLAG = 'M';

CAssetsDB::CAssetsDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "assets" / "assets", nCacheSize, fMemory, fWipe) {
}

bool CAssetsDB::WriteAssetData(const CNewAsset &asset)
{
    return Write(std::make_pair(ASSET_FLAG, asset.strName), asset);
}

bool CAssetsDB::WriteMyAssetsData(const std::string &strName, const std::set<COutPoint>& setOuts)
{
    return Write(std::make_pair(MY_ASSET_FLAG, strName), setOuts);
}

bool CAssetsDB::WriteAssetAddressQuantity(const std::string &assetName, const std::string &address, const CAmount &quantity)
{
    return Write(std::make_pair(ASSET_ADDRESS_QUANTITY_FLAG, std::make_pair(assetName, address)), quantity);
}

bool CAssetsDB::ReadAssetData(const std::string& strName, CNewAsset& asset)
{
    return Read(std::make_pair(ASSET_FLAG, strName), asset);
}

bool CAssetsDB::ReadMyAssetsData(const std::string &strName, std::set<COutPoint>& setOuts)
{
    return Read(std::make_pair(MY_ASSET_FLAG, strName), setOuts);
}

bool CAssetsDB::ReadAssetAddressQuantity(const std::string& assetName, const std::string& address, CAmount& quantity)
{
    return Read(std::make_pair(ASSET_ADDRESS_QUANTITY_FLAG, std::make_pair(assetName, address)), quantity);
}

bool CAssetsDB::EraseAssetData(const CNewAsset & asset)
{
    return Erase(std::make_pair(ASSET_FLAG, asset.strName));
}

bool CAssetsDB::EraseMyAssetData(const CNewAsset& asset)
{
    return Erase(std::make_pair(MY_ASSET_FLAG, asset.strName));
}

bool CAssetsDB::EraseAssetAddressQuantity(const std::string &assetName, const std::string &address) {
    return Erase(std::make_pair(ASSET_ADDRESS_QUANTITY_FLAG, std::make_pair(assetName, address)));
}

bool CAssetsDB::EraseMyOutPoints(const CNewAsset& asset)
{
    if (!EraseMyAssetData(asset))
        return error("%s : Failed to erase my asset outpoints from database.", __func__);

    return true;
}

bool CAssetsDB::LoadAssets()
{
    std::unique_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(std::make_pair(ASSET_FLAG, std::string()));

    // Load assets
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, std::string> key;
        if (pcursor->GetKey(key) && key.first == ASSET_FLAG) {
            CNewAsset asset;
            if (pcursor->GetValue(asset)) {
                passets->setAssets.insert(asset);
                pcursor->Next();
            } else {
                return error("%s: failed to read asset", __func__);
            }
        } else {
            break;
        }
    }

    std::unique_ptr<CDBIterator> pcursor2(NewIterator());
    pcursor2->Seek(std::make_pair(MY_ASSET_FLAG, std::string()));
    // Load mapMyUnspentAssets
    while (pcursor2->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, std::string> key;
        if (pcursor2->GetKey(key) && key.first == MY_ASSET_FLAG) {
            std::set<COutPoint> outs;
            if (pcursor2->GetValue(outs)) {
                passets->mapMyUnspentAssets.insert(std::make_pair(key.second, outs));
                pcursor2->Next();
            } else {
                return error("%s: failed to read my assets", __func__);
            }
        } else {
            break;
        }
    }

    std::unique_ptr<CDBIterator> pcursor3(NewIterator());
    pcursor3->Seek(std::make_pair(ASSET_ADDRESS_QUANTITY_FLAG, std::make_pair(std::string(), std::string())));
    // Load mapMyUnspentAssets
    while (pcursor3->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char, std::pair<std::string, std::string> > key; // <Asset Name, Address> -> Quantity
        if (pcursor3->GetKey(key) && key.first == ASSET_ADDRESS_QUANTITY_FLAG) {
            CAmount value;
            if (pcursor3->GetValue(value)) {
                if (!passets->mapAssetsAddresses.count(key.second.first)) {
                    std::set<std::string> setAddresses;
                    passets->mapAssetsAddresses.insert(std::make_pair(key.second.first, setAddresses));
                }
                if (!passets->mapAssetsAddresses[key.second.first].insert(key.second.second).second)
                    return error("%s: failed to read my address quantity from database", __func__);
                passets->mapAssetsAddressAmount.insert(std::make_pair(std::make_pair(key.second.first, key.second.second), value));
                pcursor3->Next();
            } else {
                return error("%s: failed to read my address quantity from database", __func__);
            }
        } else {
            break;
        }
    }

    return true;
}

