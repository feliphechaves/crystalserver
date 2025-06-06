////////////////////////////////////////////////////////////////////////
// Crystal Server - an opensource roleplaying game
////////////////////////////////////////////////////////////////////////
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
////////////////////////////////////////////////////////////////////////

#include "kv/kv_sql.hpp"

#include "database/database.hpp"
#include "kv/value_wrapper_proto.hpp"
#include "utils/tools.hpp"

#include <kv.pb.h>

KVSQL::KVSQL(Database &db, Logger &logger) :
	KVStore(logger), db(db) { }

std::optional<ValueWrapper> KVSQL::load(const std::string &key) {
	const auto query = fmt::format("SELECT `key_name`, `timestamp`, `value` FROM `kv_store` WHERE `key_name` = {}", db.escapeString(key));
	const auto result = db.storeQuery(query);
	if (result == nullptr) {
		return std::nullopt;
	}

	unsigned long size;
	const auto data = result->getStream("value", size);
	if (data == nullptr) {
		return std::nullopt;
	}

	ValueWrapper valueWrapper;
	const auto timestamp = result->getNumber<uint64_t>("timestamp");
	Crystal::protobuf::kv::ValueWrapper protoValue;
	if (protoValue.ParseFromArray(data, static_cast<int>(size))) {
		valueWrapper = ProtoSerializable::fromProto(protoValue, timestamp);
		return valueWrapper;
	}
	logger.error("Failed to deserialize value for key {}", key);
	return std::nullopt;
}

std::vector<std::string> KVSQL::loadPrefix(const std::string &prefix /* = ""*/) {
	std::vector<std::string> keys;
	std::string keySearch = db.escapeString(prefix + "%");
	const auto query = fmt::format("SELECT `key_name` FROM `kv_store` WHERE `key_name` LIKE {}", keySearch);
	const auto result = db.storeQuery(query);
	if (result == nullptr) {
		return keys;
	}

	do {
		std::string key = result->getString("key_name");
		replaceString(key, prefix, "");
		keys.push_back(key);
	} while (result->next());

	return keys;
}

bool KVSQL::save(const std::string &key, const ValueWrapper &value) {
	auto update = dbUpdate();
	prepareSave(key, value, update);
	return update.execute();
}

bool KVSQL::prepareSave(const std::string &key, const ValueWrapper &value, DBInsert &update) const {
	const auto protoValue = ProtoSerializable::toProto(value);
	std::string data;
	if (!protoValue.SerializeToString(&data)) {
		return false;
	}
	if (value.isDeleted()) {
		const auto query = fmt::format("DELETE FROM `kv_store` WHERE `key_name` = {}", db.escapeString(key));
		return db.executeQuery(query);
	}

	update.addRow(fmt::format("{}, {}, {}", db.escapeString(key), value.getTimestamp(), db.escapeString(data)));
	return true;
}

bool KVSQL::saveAll() {
	auto store = getStore();
	const bool success = DBTransaction::executeWithinTransaction([this, &store]() {
		auto update = dbUpdate();
		if (!std::ranges::all_of(store, [this, &update](const auto &kv) {
				const auto &[key, value] = kv;
				return prepareSave(key, value.first, update);
			})) {
			return false;
		}
		return update.execute();
	});

	if (!success) {
		g_logger().error("[{}] Error occurred saving player", __FUNCTION__);
	}

	return success;
}

DBInsert KVSQL::dbUpdate() {
	auto insert = DBInsert("INSERT INTO `kv_store` (`key_name`, `timestamp`, `value`) VALUES");
	insert.upsert({ "key_name", "timestamp", "value" });
	return insert;
}
