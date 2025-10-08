/**
 * FILE NAME: Address.h
 * 
 * DESCRIPTION: Header file for Address class
 */

#pragma once

#include <string>
#include <cstring>

/**
 * CLASS NAME: Address
 *
 * DESCRIPTION: Class representing the address of a single node
 */
class Address {
public:
	char addr[6];
	Address() {
		init();
	}
	// Copy constructor
	Address(const Address &anotherAddress);
	// Overloaded = operator
	Address& operator=(const Address &anotherAddress);
	bool operator==(const Address &anotherAddress) const;
	bool operator!=(const Address &anotherAddress) const;
	Address(std::string address) {
		size_t pos = address.find(":");
		int id = stoi(address.substr(0, pos));
		short port = (short)stoi(address.substr(pos + 1, address.size()-pos-1));
		std::memcpy(&addr[0], &id, sizeof(int));
		memcpy(&addr[4], &port, sizeof(short));
	}
	std::string toString() const {
		int id = 0;
		short port;
		memcpy(&id, &addr[0], sizeof(int));
		memcpy(&port, &addr[4], sizeof(short));
		return std::to_string(id) + ":" + std::to_string(port);
	}
	void init() {
		memset(&addr, 0, sizeof(addr));
	}
};