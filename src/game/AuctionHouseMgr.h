/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.2.5a, 4.2.3 and 5.4.8
 *
 * Copyright (C) 2005-2014  MaNGOS project <http://getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * This is part of the code that takes care of the Auction House and all that can happen with it,
 * it takes care of adding new items to it, bidding/buyouting them etc. Also handles the errors
 * that can happen, ie: you don't have enough money, your account isn't paid for (won't really
 * happen on these servers), the item you are trying to buy doesn't exist etc.
 *
 * This is also what is partly used by the \ref AuctionHouseBot as an interface to what it needs
 * for performing the usual operations such as checking what has been bidded on etc.
 *
 * \todo Add more info about how the auction house system works.
 */


#ifndef _AUCTION_HOUSE_MGR_H
#define _AUCTION_HOUSE_MGR_H

#include "Common.h"
#include "SharedDefines.h"
#include "Policies/Singleton.h"
#include "DBCStructure.h"

/** \addtogroup auctionhouse
 * @{
 * \file
 */

class Item;
class Player;
class Unit;
class WorldPacket;

#define MIN_AUCTION_TIME (2*HOUR)

/**
 * Documentation for this taken directly from comments in source
 * \todo Needs real documentation of what these values mean and where they are sent etc.
 */
enum AuctionError
{
    AUCTION_OK                          = 0,                ///< depends on enum AuctionAction
    AUCTION_ERR_INVENTORY               = 1,                ///< depends on enum InventoryChangeResult
    AUCTION_ERR_DATABASE                = 2,                ///< ERR_AUCTION_DATABASE_ERROR (default)
    AUCTION_ERR_NOT_ENOUGH_MONEY        = 3,                ///< ERR_NOT_ENOUGH_MONEY
    AUCTION_ERR_ITEM_NOT_FOUND          = 4,                ///< ERR_ITEM_NOT_FOUND
    AUCTION_ERR_HIGHER_BID              = 5,                ///< ERR_AUCTION_HIGHER_BID
    AUCTION_ERR_BID_INCREMENT           = 7,                ///< ERR_AUCTION_BID_INCREMENT
    AUCTION_ERR_BID_OWN                 = 10,               ///< ERR_AUCTION_BID_OWN
    AUCTION_ERR_RESTRICTED_ACCOUNT      = 13                ///< ERR_RESTRICTED_ACCOUNT
};

enum AuctionAction
{
    AUCTION_STARTED     = 0,                                ///< ERR_AUCTION_STARTED
    AUCTION_REMOVED     = 1,                                ///< ERR_AUCTION_REMOVED
    AUCTION_BID_PLACED  = 2                                 ///< ERR_AUCTION_BID_PLACED
};

struct AuctionEntry
{
    uint32 Id;
    uint32 itemGuidLow;                                     // can be 0 after send won mail with item
    uint32 itemTemplate;
    uint32 itemCount;
    int32 itemRandomPropertyId;
    uint32 owner;                                           // player low guid, can be 0 for server generated auction
    uint32 startbid;                                        // start minimal bid value
    uint32 bid;                                             // current bid, =0 meaning no bids
    uint32 buyout;
    time_t expireTime;
    uint32 bidder;                                          // current bidder player lowguid, can be 0 if bid generated by server, use 'bid'!=0 for check bid existance
    uint32 deposit;                                         // deposit can be calculated only when creating auction
    AuctionHouseEntry const* auctionHouseEntry;             // in AuctionHouse.dbc

    // helpers
    uint32 GetHouseId() const { return auctionHouseEntry->houseId; }
    uint32 GetHouseFaction() const { return auctionHouseEntry->faction; }
    uint32 GetAuctionCut() const;
    uint32 GetAuctionOutBid() const;
    bool BuildAuctionInfo(WorldPacket& data) const;
    void DeleteFromDB() const;
    void SaveToDB() const;
    void AuctionBidWinning(Player* bidder = NULL);
    bool UpdateBid(uint32 newbid, Player* newbidder = NULL);// true if normal bid, false if buyout, bidder==NULL for generated bid
};

// this class is used as auctionhouse instance
class AuctionHouseObject
{
    public:
        AuctionHouseObject() {}
        ~AuctionHouseObject()
        {
            for (AuctionEntryMap::const_iterator itr = AuctionsMap.begin(); itr != AuctionsMap.end(); ++itr)
                { delete itr->second; }
        }

        typedef std::map<uint32, AuctionEntry*> AuctionEntryMap;
        typedef std::pair<AuctionEntryMap::const_iterator, AuctionEntryMap::const_iterator> AuctionEntryMapBounds;

        uint32 GetCount() { return AuctionsMap.size(); }

        AuctionEntryMap const& GetAuctions() const { return AuctionsMap; }
        AuctionEntryMapBounds GetAuctionsBounds() const {return AuctionEntryMapBounds(AuctionsMap.begin(), AuctionsMap.end()); }

        void AddAuction(AuctionEntry* ah)
        {
            MANGOS_ASSERT(ah);
            AuctionsMap[ah->Id] = ah;
        }

        AuctionEntry* GetAuction(uint32 id) const
        {
            AuctionEntryMap::const_iterator itr = AuctionsMap.find(id);
            return itr != AuctionsMap.end() ? itr->second : NULL;
        }

        bool RemoveAuction(uint32 id)
        {
            return AuctionsMap.erase(id);
        }

        void Update();

        void BuildListBidderItems(WorldPacket& data, Player* player, uint32& count, uint32& totalcount);
        void BuildListOwnerItems(WorldPacket& data, Player* player, uint32& count, uint32& totalcount);
        void BuildListAuctionItems(WorldPacket& data, Player* player,
                                   std::wstring const& searchedname, uint32 listfrom, uint32 levelmin, uint32 levelmax, uint32 usable,
                                   uint32 inventoryType, uint32 itemClass, uint32 itemSubClass, uint32 quality,
                                   uint32& count, uint32& totalcount);
        AuctionEntry* AddAuction(AuctionHouseEntry const* auctionHouseEntry, Item* newItem, uint32 etime, uint32 bid, uint32 buyout = 0, uint32 deposit = 0, Player* pl = NULL);
    private:
        AuctionEntryMap AuctionsMap;
};

/**
 * This describes the type of auction house that we are dealing with, they can be either:
 * - neutral (anyone can do their shopping there)
 * - alliance/horde (only the respective faction can shop there)
 */
enum AuctionHouseType
{
    AUCTION_HOUSE_ALLIANCE  = 0, ///< Alliance only auction house
    AUCTION_HOUSE_HORDE     = 1, ///< Horde only auction house
    AUCTION_HOUSE_NEUTRAL   = 2  ///< Neutral auction house, anyone can do business here
};

#define MAX_AUCTION_HOUSE_TYPE 3

class AuctionHouseMgr
{
    public:
        AuctionHouseMgr();
        ~AuctionHouseMgr();

        typedef UNORDERED_MAP<uint32, Item*> ItemMap;

        AuctionHouseObject* GetAuctionsMap(AuctionHouseType houseType) { return &mAuctions[houseType]; }
        AuctionHouseObject* GetAuctionsMap(AuctionHouseEntry const* house);

        Item* GetAItem(uint32 id)
        {
            ItemMap::const_iterator itr = mAitems.find(id);
            if (itr != mAitems.end())
            {
                return itr->second;
            }
            return NULL;
        }

        // auction messages
        void SendAuctionWonMail(AuctionEntry* auction);
        void SendAuctionSuccessfulMail(AuctionEntry* auction);
        void SendAuctionExpiredMail(AuctionEntry* auction);
        static uint32 GetAuctionDeposit(AuctionHouseEntry const* entry, uint32 time, Item* pItem);

        static uint32 GetAuctionHouseTeam(AuctionHouseEntry const* house);
        static AuctionHouseEntry const* GetAuctionHouseEntry(Unit* unit);

    public:
        // load first auction items, because of check if item exists, when loading
        void LoadAuctionItems();
        void LoadAuctions();

        void AddAItem(Item* it);
        bool RemoveAItem(uint32 id);

        void Update();

    private:
        AuctionHouseObject  mAuctions[MAX_AUCTION_HOUSE_TYPE];

        ItemMap             mAitems;
};

/// Convenience define to access the singleton object for the Auction House Manager
#define sAuctionMgr MaNGOS::Singleton<AuctionHouseMgr>::Instance()

/** @} */

#endif
