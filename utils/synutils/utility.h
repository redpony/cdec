/*
 * utility.h
 *
 *  Created on: Jun 24, 2013
 *      Author: lijunhui
 */

#ifndef UTILITY_H_
#define UTILITY_H_


#include <zlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>

using namespace std;

typedef std::unordered_map<std::string, int> MapString2Int;
typedef std::unordered_map<std::string, float> MapString2Float;
typedef std::unordered_map<std::string, float>::iterator MapString2FloatIterator;

using namespace std;

struct SFReader {
	SFReader( ){ }
	virtual ~SFReader( ){ }

	virtual bool fnReadNextLine( char *pszLine, int* piLength ) = 0;
	virtual bool fnReadNextLine(string& strLine) = 0;
};

struct STxtFileReader: public SFReader {
	STxtFileReader(const char* pszFname) {
		m_fpIn = fopen(pszFname, "r");
		assert(m_fpIn != NULL);
	}
	~STxtFileReader() {
		if (m_fpIn != NULL)
			fclose(m_fpIn);
	}

	bool fnReadNextLine(char *pszLine, int* piLength) {
			if (feof(m_fpIn) == true)
				return false;

			int iLen;

			pszLine[ 0 ] = '\0';

			fgets(pszLine, 10001, m_fpIn);
			iLen = strlen(pszLine);
			if (iLen == 0) return false;
			while (iLen > 0 && pszLine[iLen - 1] > 0 && pszLine[iLen -1] < 33) {
				pszLine[ iLen - 1 ] = '\0';
				iLen--;
			}

			if ( piLength != NULL )
				(*piLength) = iLen;

			return true;
		}

	bool fnReadNextLine(string& strLine) {
		char *pszLine = new char[10001];
		bool bOut = fnReadNextLine(pszLine, NULL);
		if (bOut) strLine = string(pszLine);
		else strLine = string("");
		delete [] pszLine;

		return bOut;
	}
private:
	FILE *m_fpIn;
};

struct SGZFileReader: public SFReader {
	SGZFileReader(const char* pszFname) {
		m_fpIn = gzopen( pszFname, "r" );
		assert(m_fpIn != NULL);
	}
	~SGZFileReader() {
		if (m_fpIn != NULL)
			gzclose(m_fpIn);
	}

	bool fnReadNextLine(char *pszLine, int* piLength) {
		if ( m_fpIn == NULL )
			exit( 0 );
		if ( gzeof( m_fpIn ) == true )
			return false;

		int iLen;

		pszLine[ 0 ] = '\0';

		gzgets( m_fpIn, pszLine, 10001 );
		iLen = strlen( pszLine );
		while ( iLen > 0 && pszLine[ iLen - 1 ] > 0 && pszLine[ iLen -1 ] < 33 )
		{
			pszLine[ iLen - 1 ] = '\0';
			iLen--;
		}

		if ( piLength != NULL )
			(*piLength) = iLen;

		return true;
	}

	bool fnReadNextLine(string& strLine) {
		char *pszLine = new char[10001];
		bool bOut = fnReadNextLine(pszLine, NULL);
		if (bOut) strLine = string(pszLine);
		else strLine = string("");
		delete [] pszLine;

		return bOut;
	}
private:
	gzFile m_fpIn;
};


#endif /* UTILITY_H_ */

