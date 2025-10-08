

#include "Address.h"

/**
 * Copy constructor
 */
Address::Address(const Address &anotherAddress) {
	// strcpy(addr, anotherAddress.addr);
	memcpy(&addr, &anotherAddress.addr, sizeof(addr));
}

/**
 * Assignment operator overloading
 */
Address& Address::operator=(const Address& anotherAddress) {
	// strcpy(addr, anotherAddress.addr);
	memcpy(&addr, &anotherAddress.addr, sizeof(addr));
	return *this;
}

/**
 * Compare two Address objects
 */
bool Address::operator==(const Address& anotherAddress) const {
	return 0==memcmp(this->addr, anotherAddress.addr, sizeof(this->addr));
}

bool Address::operator!=(const Address& anotherAddress) const {
	return 0!=memcmp(this->addr, anotherAddress.addr, sizeof(this->addr));
}