/*
 * tree.h
 *
 *  Created on: May 23, 2013
 *      Author: lijunhui
 */

#ifndef TREE_H_
#define TREE_H_

#include <string>
#include <assert.h>
#include <stdio.h>

using namespace std;

struct STreeItem{
	STreeItem(const char* pszTerm) {
		m_pszTerm = new char[ strlen( pszTerm ) + 1 ];
		strcpy( m_pszTerm, pszTerm );

		m_ptParent = NULL;
		m_iBegin = -1;
		m_iEnd = -1;
		m_iHeadChild = -1;
		m_iHeadWord = -1;
		m_iBrotherIndex = -1;
	}
	~STreeItem( ) {
		delete [] m_pszTerm;
		for (size_t i = 0; i < m_vecChildren.size(); i++)
			delete m_vecChildren[i];
	}
	int fnAppend(STreeItem *ptChild) {
		m_vecChildren.push_back(ptChild);
		ptChild->m_iBrotherIndex = m_vecChildren.size() - 1;
		ptChild->m_ptParent = this;
		return m_vecChildren.size() - 1;
	}
	int fnGetChildrenNum() {
		return m_vecChildren.size();
	}

	bool fnIsPreTerminal( void ) {
		int I;
		if ( this == NULL || m_vecChildren.size() == 0 )
			return false;

		for ( I = 0; I < m_vecChildren.size(); I++ )
			if (m_vecChildren[I]->m_vecChildren.size() > 0 )
				return false;

		return true;
	}
public:
	char *m_pszTerm;

	vector<STreeItem*> m_vecChildren;//children items
	STreeItem *m_ptParent;//the parent item

	int m_iBegin;
	int m_iEnd; //the node span words[m_iBegin, m_iEnd]
	int m_iHeadChild; //the index of its head child
	int m_iHeadWord; //the index of its head word
	int m_iBrotherIndex;//the index in his brothers
};


struct SGetHeadWord{
	typedef vector<string> CVectorStr;
	SGetHeadWord() {

	}
	~SGetHeadWord() {

	}
	int fnGetHeadWord( char *pszCFGLeft, CVectorStr vectRight ) {
		//0 indicating from right to left while 1 indicating from left to right
		char szaHeadLists[ 201 ] = "0";

		/*  //head rules for Egnlish
		if( strcmp( pszCFGLeft, "ADJP" ) == 0 )
			strcpy( szaHeadLists, "0NNS 0QP 0NN 0$ 0ADVP 0JJ 0VBN 0VBG 0ADJP 0JJR 0NP 0JJS 0DT 0FW 0RBR 0RBS 0SBAR 0RB 0" );
		else if( strcmp( pszCFGLeft, "ADVP" ) == 0 )
			strcpy( szaHeadLists, "1RB 1RBR 1RBS 1FW 1ADVP 1TO 1CD 1JJR 1JJ 1IN 1NP 1JJS 1NN 1" );
		else if( strcmp( pszCFGLeft, "CONJP" ) == 0 )
			strcpy( szaHeadLists, "1CC 1RB 1IN 1" );
		else if( strcmp( pszCFGLeft, "FRAG" ) == 0 )
			strcpy( szaHeadLists, "1" );
		else if( strcmp( pszCFGLeft, "INTJ" ) == 0 )
			strcpy( szaHeadLists, "0" );
		else if( strcmp( pszCFGLeft, "LST" ) == 0 )
			strcpy( szaHeadLists, "1LS 1: 1CLN 1" );
		else if( strcmp( pszCFGLeft, "NAC" ) == 0 )
			strcpy( szaHeadLists, "0NN 0NNS 0NNP 0NNPS 0NP 0NAC 0EX 0$ 0CD 0QP 0PRP 0VBG 0JJ 0JJS 0JJR 0ADJP 0FW 0" );
		else if( strcmp( pszCFGLeft, "PP" ) == 0 )
			strcpy( szaHeadLists, "1IN 1TO 1VBG 1VBN 1RP 1FW 1" );
		else if( strcmp( pszCFGLeft, "PRN" ) == 0 )
			strcpy( szaHeadLists, "1" );
		else if( strcmp( pszCFGLeft, "PRT" ) == 0 )
			strcpy( szaHeadLists, "1RP 1" );
		else if( strcmp( pszCFGLeft, "QP" ) == 0 )
			strcpy( szaHeadLists, "0$ 0IN 0NNS 0NN 0JJ 0RB 0DT 0CD 0NCD 0QP 0JJR 0JJS 0" );
		else if( strcmp( pszCFGLeft, "RRC" ) == 0 )
			strcpy( szaHeadLists, "1VP 1NP 1ADVP 1ADJP 1PP 1" );
		else if( strcmp( pszCFGLeft, "S" ) == 0 )
			strcpy( szaHeadLists, "0TO 0IN 0VP 0S 0SBAR 0ADJP 0UCP 0NP 0" );
		else if( strcmp( pszCFGLeft, "SBAR" ) == 0 )
			strcpy( szaHeadLists, "0WHNP 0WHPP 0WHADVP 0WHADJP 0IN 0DT 0S 0SQ 0SINV 0SBAR 0FRAG 0" );
		else if( strcmp( pszCFGLeft, "SBARQ" ) == 0 )
			strcpy( szaHeadLists, "0SQ 0S 0SINV 0SBARQ 0FRAG 0" );
		else if( strcmp( pszCFGLeft, "SINV" ) == 0 )
			strcpy( szaHeadLists, "0VBZ 0VBD 0VBP 0VB 0MD 0VP 0S 0SINV 0ADJP 0NP 0" );
		else if( strcmp( pszCFGLeft, "SQ" ) == 0 )
			strcpy( szaHeadLists, "0VBZ 0VBD 0VBP 0VB 0MD 0VP 0SQ 0" );
		else if( strcmp( pszCFGLeft, "UCP" ) == 0 )
			strcpy( szaHeadLists, "1" );
		else if( strcmp( pszCFGLeft, "VP" ) == 0 )
			strcpy( szaHeadLists, "0TO 0VBD 0VBN 0MD 0VBZ 0VB 0VBG 0VBP 0VP 0ADJP 0NN 0NNS 0NP 0" );
		else if( strcmp( pszCFGLeft, "WHADJP" ) == 0 )
			strcpy( szaHeadLists, "0CC 0WRB 0JJ 0ADJP 0" );
		else if( strcmp( pszCFGLeft, "WHADVP" ) == 0 )
			strcpy( szaHeadLists, "1CC 1WRB 1" );
		else if( strcmp( pszCFGLeft, "WHNP" ) == 0 )
			strcpy( szaHeadLists, "0WDT 0WP 0WP$ 0WHADJP 0WHPP 0WHNP 0" );
		else if( strcmp( pszCFGLeft, "WHPP" ) == 0 )
			strcpy( szaHeadLists, "1IN 1TO FW 1" );
		else if( strcmp( pszCFGLeft, "NP" ) == 0 )
			strcpy( szaHeadLists, "0NN NNP NNS NNPS NX POS JJR 0NP 0$ ADJP PRN 0CD 0JJ JJS RB QP 0" );
		*/

		if( strcmp( pszCFGLeft, "ADJP" ) == 0 )
			strcpy( szaHeadLists, "0ADJP JJ 0AD NN CS 0" );
		else if( strcmp( pszCFGLeft, "ADVP" ) == 0 )
			strcpy( szaHeadLists, "0ADVP AD 0" );
		else if( strcmp( pszCFGLeft, "CLP" ) == 0 )
			strcpy( szaHeadLists, "0CLP M 0" );
		else if( strcmp( pszCFGLeft, "CP" ) == 0 )
			strcpy( szaHeadLists, "0DEC SP 1ADVP CS 0CP IP 0" );
		else if( strcmp( pszCFGLeft, "DNP" ) == 0 )
			strcpy( szaHeadLists, "0DNP DEG 0DEC 0" );
		else if( strcmp( pszCFGLeft, "DVP" ) == 0 )
			strcpy( szaHeadLists, "0DVP DEV 0" );
		else if( strcmp( pszCFGLeft, "DP" ) == 0 )
			strcpy( szaHeadLists, "1DP DT 1" );
		else if( strcmp( pszCFGLeft, "FRAG" ) == 0 )
			strcpy( szaHeadLists, "0VV NR NN 0" );
		else if( strcmp( pszCFGLeft, "INTJ" ) == 0 )
			strcpy( szaHeadLists, "0INTJ IJ 0" );
		else if( strcmp( pszCFGLeft, "LST" ) == 0 )
			strcpy( szaHeadLists, "1LST CD OD 1" );
		else if( strcmp( pszCFGLeft, "IP" ) == 0 )
			strcpy( szaHeadLists, "0IP VP 0VV 0" );
			//strcpy( szaHeadLists, "0VP 0VV 1IP 0" );
		else if( strcmp( pszCFGLeft, "LCP" ) == 0 )
			strcpy( szaHeadLists, "0LCP LC 0" );
		else if( strcmp( pszCFGLeft, "NP" ) == 0 )
			strcpy( szaHeadLists, "0NP NN NT NR QP 0" );
		else if( strcmp( pszCFGLeft, "PP" ) == 0 )
			strcpy( szaHeadLists, "1PP P 1" );
		else if( strcmp( pszCFGLeft, "PRN" ) == 0 )
			strcpy( szaHeadLists, "0 NP IP VP NT NR NN 0" );
		else if( strcmp( pszCFGLeft, "QP" ) == 0 )
			strcpy( szaHeadLists, "0QP CLP CD OD 0" );
		else if( strcmp( pszCFGLeft, "VP" ) == 0 )
			strcpy( szaHeadLists, "1VP VA VC VE VV BA LB VCD VSB VRD VNV VCP 1" );
		else if( strcmp( pszCFGLeft, "VCD" ) == 0 )
			strcpy( szaHeadLists, "0VCD VV VA VC VE 0" );
		if( strcmp( pszCFGLeft, "VRD" ) == 0 )
			strcpy( szaHeadLists, "0VRD VV VA VC VE 0" );
		else if( strcmp( pszCFGLeft, "VSB" ) == 0 )
			strcpy( szaHeadLists, "0VSB VV VA VC VE 0" );
		else if( strcmp( pszCFGLeft, "VCP" ) == 0 )
			strcpy( szaHeadLists, "0VCP VV VA VC VE 0" );
		else if( strcmp( pszCFGLeft, "VNV" ) == 0 )
			strcpy( szaHeadLists, "0VNV VV VA VC VE 0" );
		else if( strcmp( pszCFGLeft, "VPT" ) == 0 )
			strcpy( szaHeadLists, "0VNV VV VA VC VE 0" );
		else if( strcmp( pszCFGLeft, "UCP" ) == 0 )
			strcpy( szaHeadLists, "0" );
		else if( strcmp( pszCFGLeft, "WHNP" ) == 0 )
			strcpy( szaHeadLists, "0WHNP NP NN NT NR QP 0" );
		else if( strcmp( pszCFGLeft, "WHPP" ) == 0 )
			strcpy( szaHeadLists, "1WHPP PP P 1" );

		/*  //head rules for GENIA corpus
		if( strcmp( pszCFGLeft, "ADJP" ) == 0 )
			strcpy( szaHeadLists, "0NNS 0QP 0NN 0$ 0ADVP 0JJ 0VBN 0VBG 0ADJP 0JJR 0NP 0JJS 0DT 0FW 0RBR 0RBS 0SBAR 0RB 0" );
		else if( strcmp( pszCFGLeft, "ADVP" ) == 0 )
			strcpy( szaHeadLists, "1RB 1RBR 1RBS 1FW 1ADVP 1TO 1CD 1JJR 1JJ 1IN 1NP 1JJS 1NN 1" );
		else if( strcmp( pszCFGLeft, "CONJP" ) == 0 )
			strcpy( szaHeadLists, "1CC 1RB 1IN 1" );
		else if( strcmp( pszCFGLeft, "FRAG" ) == 0 )
			strcpy( szaHeadLists, "1" );
		else if( strcmp( pszCFGLeft, "INTJ" ) == 0 )
			strcpy( szaHeadLists, "0" );
		else if( strcmp( pszCFGLeft, "LST" ) == 0 )
			strcpy( szaHeadLists, "1LS 1: 1CLN 1" );
		else if( strcmp( pszCFGLeft, "NAC" ) == 0 )
			strcpy( szaHeadLists, "0NN 0NNS 0NNP 0NNPS 0NP 0NAC 0EX 0$ 0CD 0QP 0PRP 0VBG 0JJ 0JJS 0JJR 0ADJP 0FW 0" );
		else if( strcmp( pszCFGLeft, "PP" ) == 0 )
			strcpy( szaHeadLists, "1IN 1TO 1VBG 1VBN 1RP 1FW 1" );
		else if( strcmp( pszCFGLeft, "PRN" ) == 0 )
			strcpy( szaHeadLists, "1" );
		else if( strcmp( pszCFGLeft, "PRT" ) == 0 )
			strcpy( szaHeadLists, "1RP 1" );
		else if( strcmp( pszCFGLeft, "QP" ) == 0 )
			strcpy( szaHeadLists, "0$ 0IN 0NNS 0NN 0JJ 0RB 0DT 0CD 0NCD 0QP 0JJR 0JJS 0" );
		else if( strcmp( pszCFGLeft, "RRC" ) == 0 )
			strcpy( szaHeadLists, "1VP 1NP 1ADVP 1ADJP 1PP 1" );
		else if( strcmp( pszCFGLeft, "S" ) == 0 )
			strcpy( szaHeadLists, "0TO 0IN 0VP 0S 0SBAR 0ADJP 0UCP 0NP 0" );
		else if( strcmp( pszCFGLeft, "SBAR" ) == 0 )
			strcpy( szaHeadLists, "0WHNP 0WHPP 0WHADVP 0WHADJP 0IN 0DT 0S 0SQ 0SINV 0SBAR 0FRAG 0" );
		else if( strcmp( pszCFGLeft, "SBARQ" ) == 0 )
			strcpy( szaHeadLists, "0SQ 0S 0SINV 0SBARQ 0FRAG 0" );
		else if( strcmp( pszCFGLeft, "SINV" ) == 0 )
			strcpy( szaHeadLists, "0VBZ 0VBD 0VBP 0VB 0MD 0VP 0S 0SINV 0ADJP 0NP 0" );
		else if( strcmp( pszCFGLeft, "SQ" ) == 0 )
			strcpy( szaHeadLists, "0VBZ 0VBD 0VBP 0VB 0MD 0VP 0SQ 0" );
		else if( strcmp( pszCFGLeft, "UCP" ) == 0 )
			strcpy( szaHeadLists, "1" );
		else if( strcmp( pszCFGLeft, "VP" ) == 0 )
			strcpy( szaHeadLists, "0TO 0VBD 0VBN 0MD 0VBZ 0VB 0VBG 0VBP 0VP 0ADJP 0NN 0NNS 0NP 0" );
		else if( strcmp( pszCFGLeft, "WHADJP" ) == 0 )
			strcpy( szaHeadLists, "0CC 0WRB 0JJ 0ADJP 0" );
		else if( strcmp( pszCFGLeft, "WHADVP" ) == 0 )
			strcpy( szaHeadLists, "1CC 1WRB 1" );
		else if( strcmp( pszCFGLeft, "WHNP" ) == 0 )
			strcpy( szaHeadLists, "0WDT 0WP 0WP$ 0WHADJP 0WHPP 0WHNP 0" );
		else if( strcmp( pszCFGLeft, "WHPP" ) == 0 )
			strcpy( szaHeadLists, "1IN 1TO FW 1" );
		else if( strcmp( pszCFGLeft, "NP" ) == 0 )
			strcpy( szaHeadLists, "0NN NNP NNS NNPS NX POS JJR 0NP 0$ ADJP PRN 0CD 0JJ JJS RB QP 0" );
		*/

		return fnMyOwnHeadWordRule( szaHeadLists, vectRight );
	}

private:
	int fnMyOwnHeadWordRule( char *pszaHeadLists, CVectorStr vectRight ) {
		char szHeadList[ 201 ], *p;
		char szTerm[ 101 ];
		int J;

		p = pszaHeadLists;

		int iCountRight;

		iCountRight = vectRight.size( );

		szHeadList[ 0 ] = '\0';
		while( 1 ){
			szTerm[ 0 ] = '\0';
			sscanf( p, "%s", szTerm );
			if( strlen( szHeadList ) == 0 ){
				if( strcmp( szTerm, "0" ) == 0 ){
					return iCountRight - 1;
				}
				if( strcmp( szTerm, "1" ) == 0 ){
					return 0;
				}

				sprintf( szHeadList, "%c %s ", szTerm[ 0 ], szTerm + 1 );
				p = strstr( p, szTerm );
				p += strlen( szTerm );
			}
			else{
				if(   ( szTerm[ 0 ] == '0' )
					||( szTerm[ 0 ] == '1' ) ){
					if( szHeadList[ 0 ] == '0' ){
						for( J = iCountRight - 1; J >= 0; J -- ){
							sprintf( szTerm, " %s ", vectRight.at( J ).c_str( ) );
							if( strstr( szHeadList, szTerm ) != NULL )
								return J;
						}
					}
					else{
						for( J = 0; J < iCountRight; J ++ ){
							sprintf( szTerm, " %s ", vectRight.at( J ).c_str( ) );
							if(	strstr( szHeadList, szTerm ) != NULL )
								return J;
						}
					}

					szHeadList[ 0 ] = '\0';
				}
				else{
					strcat( szHeadList, szTerm );
					strcat( szHeadList, " " );

					p = strstr( p, szTerm );
					p += strlen( szTerm );
				}
			}
		}

		return 0;
	}

};

struct SParsedTree{
	SParsedTree( ) {
		m_ptRoot = NULL;
	}
	~SParsedTree( ) {
		if (m_ptRoot != NULL)
			delete m_ptRoot;
	}
	static SParsedTree* fnConvertFromString(const char* pszStr) {
		if (strcmp(pszStr, "(())") == 0)
			return NULL;
		SParsedTree* pTree = new SParsedTree();

		vector<string> vecSyn;
		fnReadSyntactic(pszStr, vecSyn);

		int iLeft = 1, iRight = 1; //# left/right parenthesis

		STreeItem *pcurrent;

		pTree->m_ptRoot = new STreeItem(vecSyn[1].c_str());

		pcurrent = pTree->m_ptRoot;

		for (size_t i = 2; i < vecSyn.size() - 1; i++) {
			if ( strcmp(vecSyn[i].c_str(), "(") == 0 )
					iLeft++;
			else if (strcmp(vecSyn[i].c_str(), ")") == 0 ) {
				iRight++;
				if (pcurrent == NULL) {
					//error
					fprintf(stderr, "ERROR in ConvertFromString\n");
					fprintf(stderr, "%s\n", pszStr);
					return NULL;
				}
				pcurrent = pcurrent->m_ptParent;
			} else {
				STreeItem *ptNewItem = new STreeItem(vecSyn[i].c_str());
				pcurrent->fnAppend( ptNewItem );
				pcurrent = ptNewItem;

				if (strcmp(vecSyn[i - 1].c_str(), "(" ) != 0
						&& strcmp(vecSyn[i - 1].c_str(), ")" ) != 0 ) {
					pTree->m_vecTerminals.push_back(ptNewItem);
					pcurrent = pcurrent->m_ptParent;
				}
			}
		}

		if ( iLeft != iRight ) {
			//error
			fprintf(stderr, "the left and right parentheses are not matched!");
			fprintf(stderr, "ERROR in ConvertFromString\n");
			fprintf(stderr, "%s\n", pszStr);
			return NULL;
		}

		return pTree;
	}

	int fnGetNumWord() {
		return m_vecTerminals.size();
	}

	void fnSetSpanInfo() {
		int iNextNum = 0;
		fnSuffixTraverseSetSpanInfo(m_ptRoot, iNextNum);
	}

	void fnSetHeadWord() {
		for (size_t i = 0; i < m_vecTerminals.size(); i++)
			m_vecTerminals[i]->m_iHeadWord = i;
		SGetHeadWord *pGetHeadWord = new SGetHeadWord();
		fnSuffixTraverseSetHeadWord(m_ptRoot, pGetHeadWord);
		delete pGetHeadWord;
	}

	STreeItem *fnFindNodeForSpan(int iLeft, int iRight, bool bLowest) {
		STreeItem *pTreeItem = m_vecTerminals[iLeft];

		while (pTreeItem->m_iEnd < iRight) {
			pTreeItem = pTreeItem->m_ptParent;
			if (pTreeItem == NULL) break;
		}
		if (pTreeItem == NULL)
			return NULL;
		if (pTreeItem->m_iEnd > iRight)
			return NULL;

		assert(pTreeItem->m_iEnd == iRight);
		if (bLowest)
			return pTreeItem;

		while (pTreeItem->m_ptParent != NULL && pTreeItem->m_ptParent->fnGetChildrenNum() == 1)
			pTreeItem = pTreeItem->m_ptParent;

		return pTreeItem;
	}

private:
	void fnSuffixTraverseSetSpanInfo(STreeItem *ptItem, int& iNextNum) {
		int I;
		int iNumChildren = ptItem->fnGetChildrenNum();
		for ( I = 0; I < iNumChildren; I++ )
			fnSuffixTraverseSetSpanInfo(ptItem->m_vecChildren[ I ], iNextNum);

		if ( I == 0 )
		{
			ptItem->m_iBegin = iNextNum;
			ptItem->m_iEnd = iNextNum++;
		}
		else
		{
			ptItem->m_iBegin = ptItem->m_vecChildren[0]->m_iBegin;
			ptItem->m_iEnd = ptItem->m_vecChildren[I - 1]->m_iEnd;
		}
	}


	void fnSuffixTraverseSetHeadWord(STreeItem *ptItem, SGetHeadWord *pGetHeadWord) {
		int I, iHeadchild;

		if ( ptItem->m_vecChildren.size() == 0 )
			return;

		for ( I = 0; I < ptItem->m_vecChildren.size(); I++ )
			fnSuffixTraverseSetHeadWord(ptItem->m_vecChildren[I], pGetHeadWord);

		vector<string> vecRight;


		if ( ptItem->m_vecChildren.size() == 1 )
			iHeadchild = 0;
		else
		{
			for ( I = 0; I < ptItem->m_vecChildren.size(); I++ )
				vecRight.push_back( string( ptItem->m_vecChildren[ I ]->m_pszTerm ) );

			iHeadchild = pGetHeadWord->fnGetHeadWord( ptItem->m_pszTerm, vecRight );
		}

		ptItem->m_iHeadChild = iHeadchild;
		ptItem->m_iHeadWord = ptItem->m_vecChildren[iHeadchild]->m_iHeadWord;
	}


	static void fnReadSyntactic(const char *pszSyn, vector<string>& vec) {
		char *p;
		int I;

		int iLeftNum, iRightNum;
		char *pszTmp, *pszTerm;
		pszTmp = new char[strlen(pszSyn)];
		pszTerm = new char[strlen(pszSyn)];
		pszTmp[0] = pszTerm[0] = '\0';

		vec.clear();

		char *pszLine;
		pszLine = new char[strlen(pszSyn) + 1];
		strcpy( pszLine, pszSyn );

		char *pszLine2;

		while( 1 ) {
			while(( strlen( pszLine ) > 0 )
					&&( pszLine[ strlen( pszLine ) - 1 ] > 0 )
					&&( pszLine[ strlen( pszLine ) - 1 ] <= ' ' ) )
				pszLine[ strlen( pszLine ) - 1 ] = '\0';

			if( strlen( pszLine ) == 0 )
				break;

			//printf( "%s\n", pszLine );
			pszLine2 = pszLine;
			while( pszLine2[ 0 ] <= ' ' )
				pszLine2 ++;
			if( pszLine2[ 0 ] == '<' )
				continue;

			sscanf( pszLine2 + 1, "%s", pszTmp );

			if ( pszLine2[ 0 ] == '(' ) {
				iLeftNum = 0;
				iRightNum = 0;
			}

			p = pszLine2;
			while ( 1 ) {
				pszTerm[ 0 ] = '\0';
				sscanf( p, "%s", pszTerm );

				if( strlen( pszTerm ) == 0 )
					break;
				p = strstr( p, pszTerm );
				p += strlen( pszTerm );

				if( ( pszTerm[ 0 ] == '(' )
						||( pszTerm[ strlen( pszTerm ) - 1 ] == ')' ) ) {
					if( pszTerm[ 0 ] == '(' ) {
						vec.push_back(string("("));
						iLeftNum++;

						I = 1;
						while ( pszTerm[ I ] == '(' && pszTerm[ I ]!= '\0' ) {
							vec.push_back(string("("));
							iLeftNum++;

							I++;
						}

						if( strlen( pszTerm ) > 1 )
							vec.push_back(string(pszTerm + I));
					} else {
						char *pTmp;
						pTmp = pszTerm + strlen( pszTerm ) - 1;
						while( ( pTmp[ 0 ] == ')' ) && ( pTmp >= pszTerm ) )
							pTmp --;
						pTmp[ 1 ] = '\0';

						if( strlen( pszTerm ) > 0 )
							vec.push_back(string(pszTerm));
						pTmp += 2;

						for( I = 0; I <= (int)strlen( pTmp ); I ++ ) {
							vec.push_back(string(")"));
							iRightNum++;
						}
					}
				} else {
					char *q;
					q = strchr( pszTerm, ')' );
					if ( q != NULL ) {
						q[ 0 ] = '\0';
						if ( pszTerm[ 0 ] != '\0' )
							vec.push_back(string(pszTerm));
						vec.push_back(string(")"));
						iRightNum++;

						q++;
						while ( q[ 0 ] == ')' ) {
							vec.push_back(string(")"));
							q++;
							iRightNum ++;
						}

						while( q[ 0 ] == '(' ) {
							vec.push_back(string("("));
							q++;
							iLeftNum++;
						}

						if ( q[ 0 ] != '\0' )
							vec.push_back(string(q));
					}
					else
						vec.push_back(string(pszTerm));
				}
			}

			if (iLeftNum != iRightNum) {
				fprintf(stderr, "%s\n", pszSyn);
				assert(iLeftNum == iRightNum);
			}
			/*if ( iLeftNum != iRightNum ) {
				printf( "ERROR: left( and right ) is not matched, %d ( and %d )\n", iLeftNum, iRightNum );
				return;
			}*/


			if( vec.size() >= 2
					&& strcmp( vec[1].c_str(), "(" ) == 0 ) {
				//( (IP..) )
				std::vector<string>::iterator it;
				it = vec.begin();
				it++;
				vec.insert(it, string("ROOT"));
			}

			break;
		}

		delete [] pszLine;
		delete [] pszTmp;
		delete [] pszTerm;
	}

public:
	STreeItem *m_ptRoot;
	vector<STreeItem*>  m_vecTerminals; //the leaf nodes
};

struct SParseReader {
	SParseReader(const char* pszParse_Fname, bool bFlattened = false) :
	m_bFlattened(bFlattened){
		m_fpIn = fopen( pszParse_Fname, "r" );
		assert(m_fpIn != NULL);
	}
	~SParseReader() {
		if (m_fpIn != NULL)
			fclose(m_fpIn);
	}

	SParsedTree* fnReadNextParseTree( ) {
		SParsedTree *pTree = NULL;
		char *pszLine = new char[100001];
		int iLen;

		while (fnReadNextSentence(pszLine, &iLen) == true ) {
			if (iLen == 0)
				continue;

			pTree = SParsedTree::fnConvertFromString(pszLine);
			if (pTree == NULL) 
				break;
			if (m_bFlattened)
				fnPostProcessingFlattenedParse(pTree);
			else {
				pTree->fnSetSpanInfo();
				pTree->fnSetHeadWord();
			}
			break;
		}

		delete [] pszLine;
		return pTree;
	}

	SParsedTree* fnReadNextParseTreeWithProb(double *pProb) {
		SParsedTree *pTree = NULL;
		char *pszLine = new char[100001];
		int iLen;

		while (fnReadNextSentence(pszLine, &iLen) == true ) {
			if (iLen == 0)
				continue;

			char *p = strchr(pszLine, ' ');
			assert(p != NULL);
			p[0] = '\0';
			p++;
			if (pProb)
				(*pProb) = atof(pszLine);

			pTree = SParsedTree::fnConvertFromString(p);
			if (m_bFlattened)
				fnPostProcessingFlattenedParse(pTree);
			else {
				pTree->fnSetSpanInfo();
				pTree->fnSetHeadWord();
			}
			break;
		}

		delete [] pszLine;
		return pTree;
	}
private:
	/*
	 * since to the parse tree is a flattened tree, use the head mark to identify head info.
	 * the head node will be marked as "*XP*"
	 */
	void fnSetParseTreeHeadInfo(SParsedTree *pTree) {
		for (size_t i = 0; i < pTree->m_vecTerminals.size(); i++)
			pTree->m_vecTerminals[i]->m_iHeadWord = i;
		fnSuffixTraverseSetHeadWord(pTree->m_ptRoot);
	}

	void fnSuffixTraverseSetHeadWord(STreeItem *pTreeItem) {
		if (pTreeItem->m_vecChildren.size() == 0)
			return;

		for (size_t i = 0; i < pTreeItem->m_vecChildren.size(); i++ )
			fnSuffixTraverseSetHeadWord(pTreeItem->m_vecChildren[i]);

		vector<string> vecRight;

		int iHeadchild;

		if (pTreeItem->fnIsPreTerminal()) {
			iHeadchild = 0;
		} else {
			size_t i;
			for (i = 0; i < pTreeItem->m_vecChildren.size(); i++) {
				char *p = pTreeItem->m_vecChildren[i]->m_pszTerm;
				if (p[0] == '*' && p[strlen(p) - 1] == '*') {
					iHeadchild = i;
					p[strlen(p) - 1] = '\0';
					string str = p + 1;
					strcpy(p, str.c_str()); //erase the "*..*"
					break;
				}
			}
			assert(i < pTreeItem->m_vecChildren.size());
		}

		pTreeItem->m_iHeadChild = iHeadchild;
		pTreeItem->m_iHeadWord = pTreeItem->m_vecChildren[iHeadchild]->m_iHeadWord;
	}
	void fnPostProcessingFlattenedParse(SParsedTree *pTree) {
		pTree->fnSetSpanInfo();
		fnSetParseTreeHeadInfo(pTree);
	}
	bool fnReadNextSentence(char *pszLine, int* piLength) {
		if (feof(m_fpIn) == true)
			return false;

		int iLen;

		pszLine[ 0 ] = '\0';

		fgets(pszLine, 10001, m_fpIn);
		iLen = strlen(pszLine);
		while (iLen > 0 && pszLine[iLen - 1] > 0 && pszLine[iLen -1] < 33) {
			pszLine[ iLen - 1 ] = '\0';
			iLen--;
		}

		if ( piLength != NULL )
			(*piLength) = iLen;

		return true;
	}
private:
	FILE *m_fpIn;
	const bool m_bFlattened;
};

#endif /* TREE_H_ */
