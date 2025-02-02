//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) Inspiration Byte
// 2009-2020
//////////////////////////////////////////////////////////////////////////////////
// Description: Dynamic bit array
//////////////////////////////////////////////////////////////////////////////////

#pragma once

template <int p>
constexpr int getIntExp(int _x = 0) { return getIntExp<p / 2>(_x + 1); }

template <>
constexpr int getIntExp<0>(int _x) { return _x - 1; }

static constexpr int numBitsSet(uint x)
{
	x = x - ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	x = (x + (x >> 4)) & 0x0F0F0F0F;
	x = x + (x >> 8);
	x = x + (x >> 16);
	return x & 0x0000003F;
}

class BitArray
{
	using STORAGE_TYPE = int;
public:
	BitArray(const PPSourceLine& sl, int initialSize = 64);
	~BitArray();

	const bool				operator[](int index) const;
	BitArray&				operator=(const BitArray& other);

	// cleans all bits to zero
	void					clear();

	// resizes the list
	void					resize(int newSize);

	// returns total number of bits
	int						numBits() const;

	// returns total number of bits that are set to true
	int						numTrue() const;

	// returns total number of bits that are set to false
	int						numFalse() const;

	// set the bit value
	void					set(int index, bool value);

	void					setTrue(int index);
	void					setFalse(int index);

	STORAGE_TYPE*			ptr() { return m_pListPtr; }
	const STORAGE_TYPE*		ptr() const { return m_pListPtr; }

private:
	STORAGE_TYPE&			getV(int);
	const STORAGE_TYPE&		getV(int) const;

	STORAGE_TYPE*			m_pListPtr{ nullptr };
	int						m_nSize{ 0 };
	const PPSourceLine		m_sl;
};

inline BitArray::BitArray(const PPSourceLine& sl, int initialSize)
	: m_sl(sl)
{
	resize(initialSize);
}

inline BitArray::~BitArray()
{
	delete[] m_pListPtr;
}

inline BitArray& BitArray::operator=(const BitArray& other)
{
	resize(other.m_nSize);

	const int typeSize = m_nSize / sizeof(STORAGE_TYPE);
	if (typeSize)
	{
		for (int i = 0; i < typeSize; i++)
			m_pListPtr[i] = other.m_pListPtr[i];
	}

	return *this;
}

// cleans all bits to zero
inline void BitArray::clear()
{
	memset(m_pListPtr, 0, m_nSize / sizeof(STORAGE_TYPE));
}

// resizes the list
inline void BitArray::resize(int newSize)
{
	// not changing the elemCount, so just exit
	if (newSize == m_nSize)
		return;

	const int oldTypeSize = m_nSize / sizeof(STORAGE_TYPE);
	const int newTypeSize = newSize / sizeof(STORAGE_TYPE);

	STORAGE_TYPE* temp = m_pListPtr;

	// copy the old m_pListPtr into our new one
	m_pListPtr = PPNewSL(m_sl) STORAGE_TYPE[newTypeSize];
	m_nSize = newSize;

	if (temp)
	{
		for (int i = 0; i < oldTypeSize; i++)
			m_pListPtr[i] = temp[i];
		delete[] temp;
	}

	for (int i = oldTypeSize; i < newTypeSize; ++i)
		m_pListPtr[i] = 0;
}

// returns number of bits
inline int BitArray::numBits() const
{
	return m_nSize;
}

// returns total number of bits that are set to true
inline int BitArray::numTrue() const
{
	int count = 0;
	const int typeSize = m_nSize / sizeof(STORAGE_TYPE);
	if (typeSize)
	{
		for (int i = 0; i < typeSize; i++)
			count += numBitsSet(static_cast<uint>(m_pListPtr[i]));
	}
	return count;
}

// returns total number of bits that are set to false
inline int BitArray::numFalse() const
{
	int count = 0;
	const int typeSize = m_nSize / sizeof(STORAGE_TYPE);
	if (typeSize)
	{
		for (int i = 0; i < typeSize; i++)
			count += numBitsSet(~static_cast<uint>(m_pListPtr[i]));
	}
	return count;
}

// set the bit value
inline void BitArray::set(int index, bool value)
{
	if (value) 
		setTrue(index);
	else 
		setFalse(index);
}

inline BitArray::STORAGE_TYPE& BitArray::getV(int index)
{
	constexpr const int shiftVal = getIntExp<sizeof(STORAGE_TYPE) * 8>();
	return m_pListPtr[index >> shiftVal];
}

inline const BitArray::STORAGE_TYPE& BitArray::getV(int index) const
{
	constexpr const int shiftVal = getIntExp<sizeof(STORAGE_TYPE) * 8>();
	return m_pListPtr[index >> shiftVal];
}

inline void BitArray::setTrue(int index)
{
	const int bitIndex = index & (sizeof(STORAGE_TYPE) * 8 - 1);
	getV(index) |= (1 << bitIndex);
}

inline void BitArray::setFalse(int index)
{
	const int bitIndex = index & (sizeof(STORAGE_TYPE) * 8 - 1);
	getV(index) &= ~(1 << bitIndex);
}

inline const bool BitArray::operator[](int index) const
{
	const int bitIndex = index & (sizeof(STORAGE_TYPE) * 8 - 1);
	const STORAGE_TYPE set = getV(index);
	return getV(index) & (1 << bitIndex);
}