local leaveHouse = TalkAction("!leavehouse")

function leaveHouse.onSay(player, words, param)
	if configManager.getBoolean(configKeys.CYCLOPEDIA_HOUSE_AUCTION) then
		player:sendTextMessage(MESSAGE_FAILURE, "Command have been disabled by the administrator.")
		return true
	end

	local playerPosition = player:getPosition()
	local playerTile = Tile(playerPosition)
	local house = playerTile and playerTile:getHouse()
	if not house then
		player:sendCancelMessage("You are not inside a house.")
		playerPosition:sendMagicEffect(CONST_ME_POFF)
		return true
	end

	if house:getOwnerGuid() ~= player:getGuid() then
		player:sendCancelMessage("You are not the owner of this house.")
		playerPosition:sendMagicEffect(CONST_ME_POFF)
		return true
	end

	if house:hasNewOwnership() then
		player:sendTextMessage(MESSAGE_EVENT_ADVANCE, "You cannot leave this house. Ownership is already scheduled to be transferred upon the next server restart.")
		playerPosition:sendMagicEffect(CONST_ME_POFF)
		return true
	end

	-- Move hireling back to lamp
	local tiles = house:getTiles()
	if tiles then
		for i, tile in pairs(tiles) do
			if tile then
				local position = Position(tile:getPosition())
				local hireling = getHirelingByPosition(position)
				if hireling then
					hireling:returnToLamp(player:getGuid())
				end
			end
		end
	end

	house:setNewOwnerGuid(0)
	player:sendTextMessage(MESSAGE_EVENT_ADVANCE, "You have successfully left your house.")
	playerPosition:sendMagicEffect(CONST_ME_POFF)
	return true
end

leaveHouse:separator(" ")
leaveHouse:groupType("normal")
leaveHouse:register()
