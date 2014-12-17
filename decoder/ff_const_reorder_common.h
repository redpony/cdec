#ifndef _FF_CONST_REORDER_COMMON_H
#define _FF_CONST_REORDER_COMMON_H

#include <string>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

#include "maxent.h"
#include "stringlib.h"

namespace const_reorder {

struct STreeItem {
  STreeItem(const char *pszTerm) {
    m_pszTerm = new char[strlen(pszTerm) + 1];
    strcpy(m_pszTerm, pszTerm);

    m_ptParent = NULL;
    m_iBegin = -1;
    m_iEnd = -1;
    m_iHeadChild = -1;
    m_iHeadWord = -1;
    m_iBrotherIndex = -1;
  }
  ~STreeItem() {
    delete[] m_pszTerm;
    for (size_t i = 0; i < m_vecChildren.size(); i++) delete m_vecChildren[i];
  }
  int fnAppend(STreeItem *ptChild) {
    m_vecChildren.push_back(ptChild);
    ptChild->m_iBrotherIndex = m_vecChildren.size() - 1;
    ptChild->m_ptParent = this;
    return m_vecChildren.size() - 1;
  }
  int fnGetChildrenNum() { return m_vecChildren.size(); }

  bool fnIsPreTerminal(void) {
    int I;
    if (this == NULL || m_vecChildren.size() == 0) return false;

    for (I = 0; I < m_vecChildren.size(); I++)
      if (m_vecChildren[I]->m_vecChildren.size() > 0) return false;

    return true;
  }

 public:
  char *m_pszTerm;

  std::vector<STreeItem *> m_vecChildren;  // children items
  STreeItem *m_ptParent;                   // the parent item

  int m_iBegin;
  int m_iEnd;           // the node span words[m_iBegin, m_iEnd]
  int m_iHeadChild;     // the index of its head child
  int m_iHeadWord;      // the index of its head word
  int m_iBrotherIndex;  // the index in his brothers
};

struct SGetHeadWord {
  typedef std::vector<std::string> CVectorStr;
  SGetHeadWord() {}
  ~SGetHeadWord() {}
  int fnGetHeadWord(char *pszCFGLeft, CVectorStr vectRight) {
    // 0 indicating from right to left while 1 indicating from left to right
    char szaHeadLists[201] = "0";

    /*  //head rules for Egnlish
    if( strcmp( pszCFGLeft, "ADJP" ) == 0 )
            strcpy( szaHeadLists, "0NNS 0QP 0NN 0$ 0ADVP 0JJ 0VBN 0VBG 0ADJP
    0JJR 0NP 0JJS 0DT 0FW 0RBR 0RBS 0SBAR 0RB 0" );
    else if( strcmp( pszCFGLeft, "ADVP" ) == 0 )
            strcpy( szaHeadLists, "1RB 1RBR 1RBS 1FW 1ADVP 1TO 1CD 1JJR 1JJ 1IN
    1NP 1JJS 1NN 1" );
    else if( strcmp( pszCFGLeft, "CONJP" ) == 0 )
            strcpy( szaHeadLists, "1CC 1RB 1IN 1" );
    else if( strcmp( pszCFGLeft, "FRAG" ) == 0 )
            strcpy( szaHeadLists, "1" );
    else if( strcmp( pszCFGLeft, "INTJ" ) == 0 )
            strcpy( szaHeadLists, "0" );
    else if( strcmp( pszCFGLeft, "LST" ) == 0 )
            strcpy( szaHeadLists, "1LS 1: 1CLN 1" );
    else if( strcmp( pszCFGLeft, "NAC" ) == 0 )
            strcpy( szaHeadLists, "0NN 0NNS 0NNP 0NNPS 0NP 0NAC 0EX 0$ 0CD 0QP
    0PRP 0VBG 0JJ 0JJS 0JJR 0ADJP 0FW 0" );
    else if( strcmp( pszCFGLeft, "PP" ) == 0 )
            strcpy( szaHeadLists, "1IN 1TO 1VBG 1VBN 1RP 1FW 1" );
    else if( strcmp( pszCFGLeft, "PRN" ) == 0 )
            strcpy( szaHeadLists, "1" );
    else if( strcmp( pszCFGLeft, "PRT" ) == 0 )
            strcpy( szaHeadLists, "1RP 1" );
    else if( strcmp( pszCFGLeft, "QP" ) == 0 )
            strcpy( szaHeadLists, "0$ 0IN 0NNS 0NN 0JJ 0RB 0DT 0CD 0NCD 0QP 0JJR
    0JJS 0" );
    else if( strcmp( pszCFGLeft, "RRC" ) == 0 )
            strcpy( szaHeadLists, "1VP 1NP 1ADVP 1ADJP 1PP 1" );
    else if( strcmp( pszCFGLeft, "S" ) == 0 )
            strcpy( szaHeadLists, "0TO 0IN 0VP 0S 0SBAR 0ADJP 0UCP 0NP 0" );
    else if( strcmp( pszCFGLeft, "SBAR" ) == 0 )
            strcpy( szaHeadLists, "0WHNP 0WHPP 0WHADVP 0WHADJP 0IN 0DT 0S 0SQ
    0SINV 0SBAR 0FRAG 0" );
    else if( strcmp( pszCFGLeft, "SBARQ" ) == 0 )
            strcpy( szaHeadLists, "0SQ 0S 0SINV 0SBARQ 0FRAG 0" );
    else if( strcmp( pszCFGLeft, "SINV" ) == 0 )
            strcpy( szaHeadLists, "0VBZ 0VBD 0VBP 0VB 0MD 0VP 0S 0SINV 0ADJP 0NP
    0" );
    else if( strcmp( pszCFGLeft, "SQ" ) == 0 )
            strcpy( szaHeadLists, "0VBZ 0VBD 0VBP 0VB 0MD 0VP 0SQ 0" );
    else if( strcmp( pszCFGLeft, "UCP" ) == 0 )
            strcpy( szaHeadLists, "1" );
    else if( strcmp( pszCFGLeft, "VP" ) == 0 )
            strcpy( szaHeadLists, "0TO 0VBD 0VBN 0MD 0VBZ 0VB 0VBG 0VBP 0VP
    0ADJP 0NN 0NNS 0NP 0" );
    else if( strcmp( pszCFGLeft, "WHADJP" ) == 0 )
            strcpy( szaHeadLists, "0CC 0WRB 0JJ 0ADJP 0" );
    else if( strcmp( pszCFGLeft, "WHADVP" ) == 0 )
            strcpy( szaHeadLists, "1CC 1WRB 1" );
    else if( strcmp( pszCFGLeft, "WHNP" ) == 0 )
            strcpy( szaHeadLists, "0WDT 0WP 0WP$ 0WHADJP 0WHPP 0WHNP 0" );
    else if( strcmp( pszCFGLeft, "WHPP" ) == 0 )
            strcpy( szaHeadLists, "1IN 1TO FW 1" );
    else if( strcmp( pszCFGLeft, "NP" ) == 0 )
            strcpy( szaHeadLists, "0NN NNP NNS NNPS NX POS JJR 0NP 0$ ADJP PRN
    0CD 0JJ JJS RB QP 0" );
    */

    if (strcmp(pszCFGLeft, "ADJP") == 0)
      strcpy(szaHeadLists, "0ADJP JJ 0AD NN CS 0");
    else if (strcmp(pszCFGLeft, "ADVP") == 0)
      strcpy(szaHeadLists, "0ADVP AD 0");
    else if (strcmp(pszCFGLeft, "CLP") == 0)
      strcpy(szaHeadLists, "0CLP M 0");
    else if (strcmp(pszCFGLeft, "CP") == 0)
      strcpy(szaHeadLists, "0DEC SP 1ADVP CS 0CP IP 0");
    else if (strcmp(pszCFGLeft, "DNP") == 0)
      strcpy(szaHeadLists, "0DNP DEG 0DEC 0");
    else if (strcmp(pszCFGLeft, "DVP") == 0)
      strcpy(szaHeadLists, "0DVP DEV 0");
    else if (strcmp(pszCFGLeft, "DP") == 0)
      strcpy(szaHeadLists, "1DP DT 1");
    else if (strcmp(pszCFGLeft, "FRAG") == 0)
      strcpy(szaHeadLists, "0VV NR NN 0");
    else if (strcmp(pszCFGLeft, "INTJ") == 0)
      strcpy(szaHeadLists, "0INTJ IJ 0");
    else if (strcmp(pszCFGLeft, "LST") == 0)
      strcpy(szaHeadLists, "1LST CD OD 1");
    else if (strcmp(pszCFGLeft, "IP") == 0)
      strcpy(szaHeadLists, "0IP VP 0VV 0");
    // strcpy( szaHeadLists, "0VP 0VV 1IP 0" );
    else if (strcmp(pszCFGLeft, "LCP") == 0)
      strcpy(szaHeadLists, "0LCP LC 0");
    else if (strcmp(pszCFGLeft, "NP") == 0)
      strcpy(szaHeadLists, "0NP NN NT NR QP 0");
    else if (strcmp(pszCFGLeft, "PP") == 0)
      strcpy(szaHeadLists, "1PP P 1");
    else if (strcmp(pszCFGLeft, "PRN") == 0)
      strcpy(szaHeadLists, "0 NP IP VP NT NR NN 0");
    else if (strcmp(pszCFGLeft, "QP") == 0)
      strcpy(szaHeadLists, "0QP CLP CD OD 0");
    else if (strcmp(pszCFGLeft, "VP") == 0)
      strcpy(szaHeadLists, "1VP VA VC VE VV BA LB VCD VSB VRD VNV VCP 1");
    else if (strcmp(pszCFGLeft, "VCD") == 0)
      strcpy(szaHeadLists, "0VCD VV VA VC VE 0");
    if (strcmp(pszCFGLeft, "VRD") == 0)
      strcpy(szaHeadLists, "0VRD VV VA VC VE 0");
    else if (strcmp(pszCFGLeft, "VSB") == 0)
      strcpy(szaHeadLists, "0VSB VV VA VC VE 0");
    else if (strcmp(pszCFGLeft, "VCP") == 0)
      strcpy(szaHeadLists, "0VCP VV VA VC VE 0");
    else if (strcmp(pszCFGLeft, "VNV") == 0)
      strcpy(szaHeadLists, "0VNV VV VA VC VE 0");
    else if (strcmp(pszCFGLeft, "VPT") == 0)
      strcpy(szaHeadLists, "0VNV VV VA VC VE 0");
    else if (strcmp(pszCFGLeft, "UCP") == 0)
      strcpy(szaHeadLists, "0");
    else if (strcmp(pszCFGLeft, "WHNP") == 0)
      strcpy(szaHeadLists, "0WHNP NP NN NT NR QP 0");
    else if (strcmp(pszCFGLeft, "WHPP") == 0)
      strcpy(szaHeadLists, "1WHPP PP P 1");

    /*  //head rules for GENIA corpus
    if( strcmp( pszCFGLeft, "ADJP" ) == 0 )
            strcpy( szaHeadLists, "0NNS 0QP 0NN 0$ 0ADVP 0JJ 0VBN 0VBG 0ADJP
    0JJR 0NP 0JJS 0DT 0FW 0RBR 0RBS 0SBAR 0RB 0" );
    else if( strcmp( pszCFGLeft, "ADVP" ) == 0 )
            strcpy( szaHeadLists, "1RB 1RBR 1RBS 1FW 1ADVP 1TO 1CD 1JJR 1JJ 1IN
    1NP 1JJS 1NN 1" );
    else if( strcmp( pszCFGLeft, "CONJP" ) == 0 )
            strcpy( szaHeadLists, "1CC 1RB 1IN 1" );
    else if( strcmp( pszCFGLeft, "FRAG" ) == 0 )
            strcpy( szaHeadLists, "1" );
    else if( strcmp( pszCFGLeft, "INTJ" ) == 0 )
            strcpy( szaHeadLists, "0" );
    else if( strcmp( pszCFGLeft, "LST" ) == 0 )
            strcpy( szaHeadLists, "1LS 1: 1CLN 1" );
    else if( strcmp( pszCFGLeft, "NAC" ) == 0 )
            strcpy( szaHeadLists, "0NN 0NNS 0NNP 0NNPS 0NP 0NAC 0EX 0$ 0CD 0QP
    0PRP 0VBG 0JJ 0JJS 0JJR 0ADJP 0FW 0" );
    else if( strcmp( pszCFGLeft, "PP" ) == 0 )
            strcpy( szaHeadLists, "1IN 1TO 1VBG 1VBN 1RP 1FW 1" );
    else if( strcmp( pszCFGLeft, "PRN" ) == 0 )
            strcpy( szaHeadLists, "1" );
    else if( strcmp( pszCFGLeft, "PRT" ) == 0 )
            strcpy( szaHeadLists, "1RP 1" );
    else if( strcmp( pszCFGLeft, "QP" ) == 0 )
            strcpy( szaHeadLists, "0$ 0IN 0NNS 0NN 0JJ 0RB 0DT 0CD 0NCD 0QP 0JJR
    0JJS 0" );
    else if( strcmp( pszCFGLeft, "RRC" ) == 0 )
            strcpy( szaHeadLists, "1VP 1NP 1ADVP 1ADJP 1PP 1" );
    else if( strcmp( pszCFGLeft, "S" ) == 0 )
            strcpy( szaHeadLists, "0TO 0IN 0VP 0S 0SBAR 0ADJP 0UCP 0NP 0" );
    else if( strcmp( pszCFGLeft, "SBAR" ) == 0 )
            strcpy( szaHeadLists, "0WHNP 0WHPP 0WHADVP 0WHADJP 0IN 0DT 0S 0SQ
    0SINV 0SBAR 0FRAG 0" );
    else if( strcmp( pszCFGLeft, "SBARQ" ) == 0 )
            strcpy( szaHeadLists, "0SQ 0S 0SINV 0SBARQ 0FRAG 0" );
    else if( strcmp( pszCFGLeft, "SINV" ) == 0 )
            strcpy( szaHeadLists, "0VBZ 0VBD 0VBP 0VB 0MD 0VP 0S 0SINV 0ADJP 0NP
    0" );
    else if( strcmp( pszCFGLeft, "SQ" ) == 0 )
            strcpy( szaHeadLists, "0VBZ 0VBD 0VBP 0VB 0MD 0VP 0SQ 0" );
    else if( strcmp( pszCFGLeft, "UCP" ) == 0 )
            strcpy( szaHeadLists, "1" );
    else if( strcmp( pszCFGLeft, "VP" ) == 0 )
            strcpy( szaHeadLists, "0TO 0VBD 0VBN 0MD 0VBZ 0VB 0VBG 0VBP 0VP
    0ADJP 0NN 0NNS 0NP 0" );
    else if( strcmp( pszCFGLeft, "WHADJP" ) == 0 )
            strcpy( szaHeadLists, "0CC 0WRB 0JJ 0ADJP 0" );
    else if( strcmp( pszCFGLeft, "WHADVP" ) == 0 )
            strcpy( szaHeadLists, "1CC 1WRB 1" );
    else if( strcmp( pszCFGLeft, "WHNP" ) == 0 )
            strcpy( szaHeadLists, "0WDT 0WP 0WP$ 0WHADJP 0WHPP 0WHNP 0" );
    else if( strcmp( pszCFGLeft, "WHPP" ) == 0 )
            strcpy( szaHeadLists, "1IN 1TO FW 1" );
    else if( strcmp( pszCFGLeft, "NP" ) == 0 )
            strcpy( szaHeadLists, "0NN NNP NNS NNPS NX POS JJR 0NP 0$ ADJP PRN
    0CD 0JJ JJS RB QP 0" );
    */

    return fnMyOwnHeadWordRule(szaHeadLists, vectRight);
  }

 private:
  int fnMyOwnHeadWordRule(char *pszaHeadLists, CVectorStr vectRight) {
    char szHeadList[201], *p;
    char szTerm[101];
    int J;

    p = pszaHeadLists;

    int iCountRight;

    iCountRight = vectRight.size();

    szHeadList[0] = '\0';
    while (1) {
      szTerm[0] = '\0';
      sscanf(p, "%s", szTerm);
      if (strlen(szHeadList) == 0) {
        if (strcmp(szTerm, "0") == 0) {
          return iCountRight - 1;
        }
        if (strcmp(szTerm, "1") == 0) {
          return 0;
        }

        sprintf(szHeadList, "%c %s ", szTerm[0], szTerm + 1);
        p = strstr(p, szTerm);
        p += strlen(szTerm);
      } else {
        if ((szTerm[0] == '0') || (szTerm[0] == '1')) {
          if (szHeadList[0] == '0') {
            for (J = iCountRight - 1; J >= 0; J--) {
              sprintf(szTerm, " %s ", vectRight.at(J).c_str());
              if (strstr(szHeadList, szTerm) != NULL) return J;
            }
          } else {
            for (J = 0; J < iCountRight; J++) {
              sprintf(szTerm, " %s ", vectRight.at(J).c_str());
              if (strstr(szHeadList, szTerm) != NULL) return J;
            }
          }

          szHeadList[0] = '\0';
        } else {
          strcat(szHeadList, szTerm);
          strcat(szHeadList, " ");

          p = strstr(p, szTerm);
          p += strlen(szTerm);
        }
      }
    }

    return 0;
  }
};

struct SParsedTree {
  SParsedTree() { m_ptRoot = NULL; }
  ~SParsedTree() {
    if (m_ptRoot != NULL) delete m_ptRoot;
  }
  static SParsedTree *fnConvertFromString(const char *pszStr) {
    if (strcmp(pszStr, "(())") == 0) return NULL;
    SParsedTree *pTree = new SParsedTree();

    std::vector<std::string> vecSyn;
    fnReadSyntactic(pszStr, vecSyn);

    int iLeft = 1, iRight = 1;  //# left/right parenthesis

    STreeItem *pcurrent;

    pTree->m_ptRoot = new STreeItem(vecSyn[1].c_str());

    pcurrent = pTree->m_ptRoot;

    for (size_t i = 2; i < vecSyn.size() - 1; i++) {
      if (strcmp(vecSyn[i].c_str(), "(") == 0)
        iLeft++;
      else if (strcmp(vecSyn[i].c_str(), ")") == 0) {
        iRight++;
        if (pcurrent == NULL) {
          // error
          fprintf(stderr, "ERROR in ConvertFromString\n");
          fprintf(stderr, "%s\n", pszStr);
          return NULL;
        }
        pcurrent = pcurrent->m_ptParent;
      } else {
        STreeItem *ptNewItem = new STreeItem(vecSyn[i].c_str());
        pcurrent->fnAppend(ptNewItem);
        pcurrent = ptNewItem;

        if (strcmp(vecSyn[i - 1].c_str(), "(") != 0 &&
            strcmp(vecSyn[i - 1].c_str(), ")") != 0) {
          pTree->m_vecTerminals.push_back(ptNewItem);
          pcurrent = pcurrent->m_ptParent;
        }
      }
    }

    if (iLeft != iRight) {
      // error
      fprintf(stderr, "the left and right parentheses are not matched!");
      fprintf(stderr, "ERROR in ConvertFromString\n");
      fprintf(stderr, "%s\n", pszStr);
      return NULL;
    }

    return pTree;
  }

  int fnGetNumWord() { return m_vecTerminals.size(); }

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
    if (pTreeItem == NULL) return NULL;
    if (pTreeItem->m_iEnd > iRight) return NULL;

    assert(pTreeItem->m_iEnd == iRight);
    if (bLowest) return pTreeItem;

    while (pTreeItem->m_ptParent != NULL &&
           pTreeItem->m_ptParent->fnGetChildrenNum() == 1)
      pTreeItem = pTreeItem->m_ptParent;

    return pTreeItem;
  }

 private:
  void fnSuffixTraverseSetSpanInfo(STreeItem *ptItem, int &iNextNum) {
    int I;
    int iNumChildren = ptItem->fnGetChildrenNum();
    for (I = 0; I < iNumChildren; I++)
      fnSuffixTraverseSetSpanInfo(ptItem->m_vecChildren[I], iNextNum);

    if (I == 0) {
      ptItem->m_iBegin = iNextNum;
      ptItem->m_iEnd = iNextNum++;
    } else {
      ptItem->m_iBegin = ptItem->m_vecChildren[0]->m_iBegin;
      ptItem->m_iEnd = ptItem->m_vecChildren[I - 1]->m_iEnd;
    }
  }

  void fnSuffixTraverseSetHeadWord(STreeItem *ptItem,
                                   SGetHeadWord *pGetHeadWord) {
    int I, iHeadchild;

    if (ptItem->m_vecChildren.size() == 0) return;

    for (I = 0; I < ptItem->m_vecChildren.size(); I++)
      fnSuffixTraverseSetHeadWord(ptItem->m_vecChildren[I], pGetHeadWord);

    std::vector<std::string> vecRight;

    if (ptItem->m_vecChildren.size() == 1)
      iHeadchild = 0;
    else {
      for (I = 0; I < ptItem->m_vecChildren.size(); I++)
        vecRight.push_back(std::string(ptItem->m_vecChildren[I]->m_pszTerm));

      iHeadchild = pGetHeadWord->fnGetHeadWord(ptItem->m_pszTerm, vecRight);
    }

    ptItem->m_iHeadChild = iHeadchild;
    ptItem->m_iHeadWord = ptItem->m_vecChildren[iHeadchild]->m_iHeadWord;
  }

  static void fnReadSyntactic(const char *pszSyn,
                              std::vector<std::string> &vec) {
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
    strcpy(pszLine, pszSyn);

    char *pszLine2;

    while (1) {
      while ((strlen(pszLine) > 0) && (pszLine[strlen(pszLine) - 1] > 0) &&
             (pszLine[strlen(pszLine) - 1] <= ' '))
        pszLine[strlen(pszLine) - 1] = '\0';

      if (strlen(pszLine) == 0) break;

      // printf( "%s\n", pszLine );
      pszLine2 = pszLine;
      while (pszLine2[0] <= ' ') pszLine2++;
      if (pszLine2[0] == '<') continue;

      sscanf(pszLine2 + 1, "%s", pszTmp);

      if (pszLine2[0] == '(') {
        iLeftNum = 0;
        iRightNum = 0;
      }

      p = pszLine2;
      while (1) {
        pszTerm[0] = '\0';
        sscanf(p, "%s", pszTerm);

        if (strlen(pszTerm) == 0) break;
        p = strstr(p, pszTerm);
        p += strlen(pszTerm);

        if ((pszTerm[0] == '(') || (pszTerm[strlen(pszTerm) - 1] == ')')) {
          if (pszTerm[0] == '(') {
            vec.push_back(std::string("("));
            iLeftNum++;

            I = 1;
            while (pszTerm[I] == '(' && pszTerm[I] != '\0') {
              vec.push_back(std::string("("));
              iLeftNum++;

              I++;
            }

            if (strlen(pszTerm) > 1) vec.push_back(std::string(pszTerm + I));
          } else {
            char *pTmp;
            pTmp = pszTerm + strlen(pszTerm) - 1;
            while ((pTmp[0] == ')') && (pTmp >= pszTerm)) pTmp--;
            pTmp[1] = '\0';

            if (strlen(pszTerm) > 0) vec.push_back(std::string(pszTerm));
            pTmp += 2;

            for (I = 0; I <= (int)strlen(pTmp); I++) {
              vec.push_back(std::string(")"));
              iRightNum++;
            }
          }
        } else {
          char *q;
          q = strchr(pszTerm, ')');
          if (q != NULL) {
            q[0] = '\0';
            if (pszTerm[0] != '\0') vec.push_back(std::string(pszTerm));
            vec.push_back(std::string(")"));
            iRightNum++;

            q++;
            while (q[0] == ')') {
              vec.push_back(std::string(")"));
              q++;
              iRightNum++;
            }

            while (q[0] == '(') {
              vec.push_back(std::string("("));
              q++;
              iLeftNum++;
            }

            if (q[0] != '\0') vec.push_back(std::string(q));
          } else
            vec.push_back(std::string(pszTerm));
        }
      }

      if (iLeftNum != iRightNum) {
        fprintf(stderr, "%s\n", pszSyn);
        assert(iLeftNum == iRightNum);
      }
      /*if ( iLeftNum != iRightNum ) {
              printf( "ERROR: left( and right ) is not matched, %d ( and %d
      )\n", iLeftNum, iRightNum );
              return;
      }*/

      if (vec.size() >= 2 && strcmp(vec[1].c_str(), "(") == 0) {
        //( (IP..) )
        std::vector<std::string>::iterator it;
        it = vec.begin();
        it++;
        vec.insert(it, std::string("ROOT"));
      }

      break;
    }

    delete[] pszLine;
    delete[] pszTmp;
    delete[] pszTerm;
  }

 public:
  STreeItem *m_ptRoot;
  std::vector<STreeItem *> m_vecTerminals;  // the leaf nodes
};

struct SParseReader {
  SParseReader(const char *pszParse_Fname, bool bFlattened = false)
      : m_bFlattened(bFlattened) {
    m_fpIn = fopen(pszParse_Fname, "r");
    assert(m_fpIn != NULL);
  }
  ~SParseReader() {
    if (m_fpIn != NULL) fclose(m_fpIn);
  }

  SParsedTree *fnReadNextParseTree() {
    SParsedTree *pTree = NULL;
    char *pszLine = new char[100001];
    int iLen;

    while (fnReadNextSentence(pszLine, &iLen) == true) {
      if (iLen == 0) continue;

      pTree = SParsedTree::fnConvertFromString(pszLine);
      if (pTree == NULL) break;
      if (m_bFlattened)
        fnPostProcessingFlattenedParse(pTree);
      else {
        pTree->fnSetSpanInfo();
        pTree->fnSetHeadWord();
      }
      break;
    }

    delete[] pszLine;
    return pTree;
  }

  SParsedTree *fnReadNextParseTreeWithProb(double *pProb) {
    SParsedTree *pTree = NULL;
    char *pszLine = new char[100001];
    int iLen;

    while (fnReadNextSentence(pszLine, &iLen) == true) {
      if (iLen == 0) continue;

      char *p = strchr(pszLine, ' ');
      assert(p != NULL);
      p[0] = '\0';
      p++;
      if (pProb) (*pProb) = atof(pszLine);

      pTree = SParsedTree::fnConvertFromString(p);
      if (m_bFlattened)
        fnPostProcessingFlattenedParse(pTree);
      else {
        pTree->fnSetSpanInfo();
        pTree->fnSetHeadWord();
      }
      break;
    }

    delete[] pszLine;
    return pTree;
  }

 private:
  /*
   * since to the parse tree is a flattened tree, use the head mark to identify
   * head info.
   * the head node will be marked as "*XP*"
   */
  void fnSetParseTreeHeadInfo(SParsedTree *pTree) {
    for (size_t i = 0; i < pTree->m_vecTerminals.size(); i++)
      pTree->m_vecTerminals[i]->m_iHeadWord = i;
    fnSuffixTraverseSetHeadWord(pTree->m_ptRoot);
  }

  void fnSuffixTraverseSetHeadWord(STreeItem *pTreeItem) {
    if (pTreeItem->m_vecChildren.size() == 0) return;

    for (size_t i = 0; i < pTreeItem->m_vecChildren.size(); i++)
      fnSuffixTraverseSetHeadWord(pTreeItem->m_vecChildren[i]);

    std::vector<std::string> vecRight;

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
          std::string str = p + 1;
          strcpy(p, str.c_str());  // erase the "*..*"
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
  bool fnReadNextSentence(char *pszLine, int *piLength) {
    if (feof(m_fpIn) == true) return false;

    int iLen;

    pszLine[0] = '\0';

    fgets(pszLine, 10001, m_fpIn);
    iLen = strlen(pszLine);
    while (iLen > 0 && pszLine[iLen - 1] > 0 && pszLine[iLen - 1] < 33) {
      pszLine[iLen - 1] = '\0';
      iLen--;
    }

    if (piLength != NULL) (*piLength) = iLen;

    return true;
  }

 private:
  FILE *m_fpIn;
  const bool m_bFlattened;
};

/*
 * Note:
 *      m_vec_s_align.size() may not be equal to the length of source side
 *sentence
 *                           due to the last words may not be aligned
 *
 */
struct SAlignment {
  typedef std::vector<int> SingleAlign;
  SAlignment(const char* pszAlign) { fnInitializeAlignment(pszAlign); }
  ~SAlignment() {}

  bool fnIsAligned(int i, bool s) const {
    const std::vector<SingleAlign>* palign;
    if (s == true)
      palign = &m_vec_s_align;
    else
      palign = &m_vec_t_align;
    if ((*palign)[i].size() == 0) return false;
    return true;
  }

  /*
   * return true if [b, e] is aligned phrases on source side (if s==true) or on
   * the target side (if s==false);
   * return false, otherwise.
   */
  bool fnIsAlignedPhrase(int b, int e, bool s, int* pob, int* poe) const {
    int ob, oe;  //[b, e] on the other side
    if (s == true)
      fnGetLeftRightMost(b, e, m_vec_s_align, ob, oe);
    else
      fnGetLeftRightMost(b, e, m_vec_t_align, ob, oe);

    if (ob == -1) {
      if (pob != NULL) (*pob) = -1;
      if (poe != NULL) (*poe) = -1;
      return false;  // no aligned word among [b, e]
    }
    if (pob != NULL) (*pob) = ob;
    if (poe != NULL) (*poe) = oe;

    int bb, be;  //[b, e] back given [ob, oe] on the other side
    if (s == true)
      fnGetLeftRightMost(ob, oe, m_vec_t_align, bb, be);
    else
      fnGetLeftRightMost(ob, oe, m_vec_s_align, bb, be);

    if (bb < b || be > e) return false;
    return true;
  }

  bool fnIsAlignedTightPhrase(int b, int e, bool s, int* pob, int* poe) const {
    const std::vector<SingleAlign>* palign;
    if (s == true)
      palign = &m_vec_s_align;
    else
      palign = &m_vec_t_align;

    if ((*palign).size() <= e || (*palign)[b].size() == 0 ||
        (*palign)[e].size() == 0)
      return false;

    return fnIsAlignedPhrase(b, e, s, pob, poe);
  }

  void fnGetLeftRightMost(int b, int e, bool s, int& ob, int& oe) const {
    if (s == true)
      fnGetLeftRightMost(b, e, m_vec_s_align, ob, oe);
    else
      fnGetLeftRightMost(b, e, m_vec_t_align, ob, oe);
  }

  /*
   * look the translation of source[b, e] is continuous or not
   * 1) return "Unaligned": if the source[b, e] is translated silently;
   * 2) return "Con't": if none of target words in target[.., ..] is exclusively
   * aligned to any word outside source[b, e]
   * 3) return "Discon't": otherwise;
   */
  std::string fnIsContinuous(int b, int e) const {
    int ob, oe;
    fnGetLeftRightMost(b, e, true, ob, oe);
    if (ob == -1) return "Unaligned";

    for (int i = ob; i <= oe; i++) {
      if (!fnIsAligned(i, false)) continue;
      const SingleAlign& a = m_vec_t_align[i];
      int j;
      for (j = 0; j < a.size(); j++)
        if (a[j] >= b && a[j] <= e) break;
      if (j == a.size()) return "Discon't";
    }
    return "Con't";
  }

  const SingleAlign* fnGetSingleWordAlign(int i, bool s) const {
    if (s == true) {
      if (i >= m_vec_s_align.size()) return NULL;
      return &(m_vec_s_align[i]);
    } else {
      if (i >= m_vec_t_align.size()) return NULL;
      return &(m_vec_t_align[i]);
    }
  }

 private:
  void fnGetLeftRightMost(int b, int e, const std::vector<SingleAlign>& align,
                          int& ob, int& oe) const {
    ob = oe = -1;
    for (int i = b; i <= e && i < align.size(); i++) {
      if (align[i].size() > 0) {
        if (align[i][0] < ob || ob == -1) ob = align[i][0];
        if (oe < align[i][align[i].size() - 1])
          oe = align[i][align[i].size() - 1];
      }
    }
  }
  void fnInitializeAlignment(const char* pszAlign) {
    m_vec_s_align.clear();
    m_vec_t_align.clear();

    std::vector<std::string> terms = SplitOnWhitespace(std::string(pszAlign));
    int si, ti;
    for (size_t i = 0; i < terms.size(); i++) {
      sscanf(terms[i].c_str(), "%d-%d", &si, &ti);

      while (m_vec_s_align.size() <= si) {
        SingleAlign sa;
        m_vec_s_align.push_back(sa);
      }
      while (m_vec_t_align.size() <= ti) {
        SingleAlign sa;
        m_vec_t_align.push_back(sa);
      }

      m_vec_s_align[si].push_back(ti);
      m_vec_t_align[ti].push_back(si);
    }

    // sort
    for (size_t i = 0; i < m_vec_s_align.size(); i++) {
      std::sort(m_vec_s_align[i].begin(), m_vec_s_align[i].end());
    }
    for (size_t i = 0; i < m_vec_t_align.size(); i++) {
      std::sort(m_vec_t_align[i].begin(), m_vec_t_align[i].end());
    }
  }

 private:
  std::vector<SingleAlign> m_vec_s_align;  // source side words' alignment
  std::vector<SingleAlign> m_vec_t_align;  // target side words' alignment
};

struct SAlignmentReader {
  SAlignmentReader(const char* pszFname) {
    m_fpIn = fopen(pszFname, "r");
    assert(m_fpIn != NULL);
  }
  ~SAlignmentReader() {
    if (m_fpIn != NULL) fclose(m_fpIn);
  }
  SAlignment* fnReadNextAlignment() {
    if (feof(m_fpIn) == true) return NULL;
    char* pszLine = new char[100001];
    pszLine[0] = '\0';
    fgets(pszLine, 10001, m_fpIn);
    int iLen = strlen(pszLine);
    if (iLen == 0) return NULL;
    while (iLen > 0 && pszLine[iLen - 1] > 0 && pszLine[iLen - 1] < 33) {
      pszLine[iLen - 1] = '\0';
      iLen--;
    }
    SAlignment* pAlign = new SAlignment(pszLine);
    delete[] pszLine;
    return pAlign;
  }

 private:
  FILE* m_fpIn;
};

struct SArgument {
  SArgument(const char* pszRole, int iBegin, int iEnd, float fProb) {
    m_pszRole = new char[strlen(pszRole) + 1];
    strcpy(m_pszRole, pszRole);
    m_iBegin = iBegin;
    m_iEnd = iEnd;
    m_fProb = fProb;
    m_pTreeItem = NULL;
  }
  ~SArgument() { delete[] m_pszRole; }

  void fnSetTreeItem(STreeItem* pTreeItem) {
    m_pTreeItem = pTreeItem;
    if (m_pTreeItem != NULL && m_pTreeItem->m_iBegin != -1) {
      assert(m_pTreeItem->m_iBegin == m_iBegin);
      assert(m_pTreeItem->m_iEnd == m_iEnd);
    }
  }

  char* m_pszRole;  // argument rule, e.g., ARG0, ARGM-TMP
  int m_iBegin;
  int m_iEnd;     // the span of the argument, [m_iBegin, m_iEnd]
  float m_fProb;  // the probability of this role,
  STreeItem* m_pTreeItem;
};

struct SPredicate {
  SPredicate(const char* pszLemma, int iPosition) {
    if (pszLemma != NULL) {
      m_pszLemma = new char[strlen(pszLemma) + 1];
      strcpy(m_pszLemma, pszLemma);
    } else
      m_pszLemma = NULL;
    m_iPosition = iPosition;
  }
  ~SPredicate() {
    if (m_pszLemma != NULL) delete[] m_pszLemma;
    for (size_t i = 0; i < m_vecArgt.size(); i++) delete m_vecArgt[i];
  }
  int fnAppend(const char* pszRole, int iBegin, int iEnd) {
    SArgument* pArgt = new SArgument(pszRole, iBegin, iEnd, 1.0);
    return fnAppend(pArgt);
  }
  int fnAppend(SArgument* pArgt) {
    m_vecArgt.push_back(pArgt);
    int iPosition = m_vecArgt.size() - 1;
    return iPosition;
  }

  char* m_pszLemma;  // lemma of the predicate, for Chinese, it's always as same
                     // as the predicate itself
  int m_iPosition;   // the position in sentence
  std::vector<SArgument*> m_vecArgt;  // arguments associated to the predicate
};

struct SSrlSentence {
  SSrlSentence() { m_pTree = NULL; }
  ~SSrlSentence() {
    if (m_pTree != NULL) delete m_pTree;

    for (size_t i = 0; i < m_vecPred.size(); i++) delete m_vecPred[i];
  }
  int fnAppend(const char* pszLemma, int iPosition) {
    SPredicate* pPred = new SPredicate(pszLemma, iPosition);
    return fnAppend(pPred);
  }
  int fnAppend(SPredicate* pPred) {
    m_vecPred.push_back(pPred);
    int iPosition = m_vecPred.size() - 1;
    return iPosition;
  }
  int GetPredicateNum() { return m_vecPred.size(); }

  SParsedTree* m_pTree;
  std::vector<SPredicate*> m_vecPred;
};

struct SSrlSentenceReader {
  SSrlSentenceReader(const char* pszSrlFname) {
    m_fpIn = fopen(pszSrlFname, "r");
    assert(m_fpIn != NULL);
  }
  ~SSrlSentenceReader() {
    if (m_fpIn != NULL) fclose(m_fpIn);
  }

  inline void fnReplaceAll(std::string& str, const std::string& from,
                           const std::string& to) {
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
      str.replace(start_pos, from.length(), to);
      start_pos += to.length();  // In case 'to' contains 'from', like replacing
                                 // 'x' with 'yx'
    }
  }

  // TODO: here only considers flat predicate-argument structure
  //      i.e., no overlap among them
  SSrlSentence* fnReadNextSrlSentence() {
    std::vector<std::vector<std::string> > vecContent;
    if (fnReadNextContent(vecContent) == false) return NULL;

    SSrlSentence* pSrlSentence = new SSrlSentence();
    int iSize = vecContent.size();
    // put together syntactic text
    std::ostringstream ostr;
    for (int i = 0; i < iSize; i++) {
      std::string strSynSeg =
          vecContent[i][5];  // the 5th column is the syntactic segment
      size_t iPosition = strSynSeg.find_first_of('*');
      assert(iPosition != std::string::npos);
      std::ostringstream ostrTmp;
      ostrTmp << "(" << vecContent[i][2] << " " << vecContent[i][0]
              << ")";  // the 2th column is POS-tag, and the 0th column is word
      strSynSeg.replace(iPosition, 1, ostrTmp.str());
      fnReplaceAll(strSynSeg, "(", " (");
      ostr << strSynSeg;
    }
    std::string strSyn = ostr.str();
    pSrlSentence->m_pTree = SParsedTree::fnConvertFromString(strSyn.c_str());
    pSrlSentence->m_pTree->fnSetHeadWord();
    pSrlSentence->m_pTree->fnSetSpanInfo();

    // read predicate-argument structure
    int iNumPred = vecContent[0].size() - 8;
    for (int i = 0; i < iNumPred; i++) {
      std::vector<std::string> vecRole;
      std::vector<int> vecBegin;
      std::vector<int> vecEnd;
      int iPred = -1;
      for (int j = 0; j < iSize; j++) {
        const char* p = vecContent[j][i + 8].c_str();
        const char* q;
        if (p[0] == '(') {
          // starting position of an argument(or predicate)
          vecBegin.push_back(j);
          q = strchr(p, '*');
          assert(q != NULL);
          vecRole.push_back(vecContent[j][i + 8].substr(1, q - p - 1));
          if (vecRole.back().compare("V") == 0) {
            assert(iPred == -1);
            iPred = vecRole.size() - 1;
          }
        }
        if (p[strlen(p) - 1] == ')') {
          // end position of an argument(or predicate)
          vecEnd.push_back(j);
          assert(vecBegin.size() == vecEnd.size());
        }
      }
      assert(iPred != -1);
      SPredicate* pPred = new SPredicate(
          pSrlSentence->m_pTree->m_vecTerminals[vecBegin[iPred]]->m_pszTerm,
          vecBegin[iPred]);
      pSrlSentence->fnAppend(pPred);
      for (size_t j = 0; j < vecBegin.size(); j++) {
        if (j == iPred) continue;
        pPred->fnAppend(vecRole[j].c_str(), vecBegin[j], vecEnd[j]);
        pPred->m_vecArgt.back()->fnSetTreeItem(
            pSrlSentence->m_pTree->fnFindNodeForSpan(vecBegin[j], vecEnd[j],
                                                     false));
      }
    }
    return pSrlSentence;
  }

 private:
  bool fnReadNextContent(std::vector<std::vector<std::string> >& vecContent) {
    vecContent.clear();
    if (feof(m_fpIn) == true) return false;
    char* pszLine;
    pszLine = new char[100001];
    pszLine[0] = '\0';
    int iLen;
    while (!feof(m_fpIn)) {
      fgets(pszLine, 10001, m_fpIn);
      iLen = strlen(pszLine);
      while (iLen > 0 && pszLine[iLen - 1] > 0 && pszLine[iLen - 1] < 33) {
        pszLine[iLen - 1] = '\0';
        iLen--;
      }
      if (iLen == 0) break;  // end of this sentence

      std::vector<std::string> terms = SplitOnWhitespace(std::string(pszLine));
      assert(terms.size() > 7);
      vecContent.push_back(terms);
    }
    delete[] pszLine;
    return true;
  }

 private:
  FILE* m_fpIn;
};

typedef std::unordered_map<std::string, int> Map;
typedef std::unordered_map<std::string, int>::iterator Iterator;

struct Tsuruoka_Maxent {
  Tsuruoka_Maxent(const char* pszModelFName) {
    if (pszModelFName != NULL) {
      m_pModel = new maxent::ME_Model();
      m_pModel->load_from_file(pszModelFName);
    } else
      m_pModel = NULL;
  }

  ~Tsuruoka_Maxent() {
    if (m_pModel != NULL) delete m_pModel;
  }

  void fnEval(const char* pszContext, std::vector<double>& vecOutput) const {
    std::vector<std::string> vecContext;
    maxent::ME_Sample* pmes = new maxent::ME_Sample();
    SplitOnWhitespace(std::string(pszContext), &vecContext);

    vecOutput.clear();

    for (size_t i = 0; i < vecContext.size(); i++)
      pmes->add_feature(vecContext[i]);
    std::vector<double> vecProb = m_pModel->classify(*pmes);

    for (size_t i = 0; i < vecProb.size(); i++) {
      std::string label = m_pModel->get_class_label(i);
      vecOutput.push_back(vecProb[i]);
    }
    delete pmes;
  }
  int fnGetClassId(const std::string& strLabel) const {
    return m_pModel->get_class_id(strLabel);
  }

 private:
  maxent::ME_Model* m_pModel;
};

// an argument item or a predicate item (the verb itself)
struct SSRLItem {
  SSRLItem(const STreeItem *tree_item, std::string role)
      : tree_item_(tree_item), role_(role) {}
  ~SSRLItem() {}
  const STreeItem *tree_item_;
  const std::string role_;
};

struct SPredicateItem {
  SPredicateItem(const SParsedTree *tree, const SPredicate *pred)
      : pred_(pred) {
    vec_items_.reserve(pred->m_vecArgt.size() + 1);
    for (int i = 0; i < pred->m_vecArgt.size(); i++) {
      vec_items_.push_back(
          new SSRLItem(pred->m_vecArgt[i]->m_pTreeItem,
                       std::string(pred->m_vecArgt[i]->m_pszRole)));
    }
    vec_items_.push_back(
        new SSRLItem(tree->m_vecTerminals[pred->m_iPosition]->m_ptParent,
                     std::string("Pred")));
    sort(vec_items_.begin(), vec_items_.end(), SortFunction);

    begin_ = vec_items_[0]->tree_item_->m_iBegin;
    end_ = vec_items_[vec_items_.size() - 1]->tree_item_->m_iEnd;
  }

  ~SPredicateItem() { vec_items_.clear(); }

  static bool SortFunction(SSRLItem *i, SSRLItem *j) {
    return (i->tree_item_->m_iBegin < j->tree_item_->m_iBegin);
  }

  std::vector<SSRLItem *> vec_items_;
  int begin_;
  int end_;
  const SPredicate *pred_;
};

struct SArgumentReorderModel {
 public:
  static std::string fnGetBlockOutcome(int iBegin, int iEnd,
                                       SAlignment *pAlign) {
    return pAlign->fnIsContinuous(iBegin, iEnd);
  }
  static void fnGetReorderType(SPredicateItem *pPredItem, SAlignment *pAlign,
                               std::vector<std::string> &vecStrLeftReorder,
                               std::vector<std::string> &vecStrRightReorder) {
    std::vector<int> vecLeft, vecRight;
    for (int i = 0; i < pPredItem->vec_items_.size(); i++) {
      const STreeItem *pCon1 = pPredItem->vec_items_[i]->tree_item_;
      int iLeft1, iRight1;
      pAlign->fnGetLeftRightMost(pCon1->m_iBegin, pCon1->m_iEnd, true, iLeft1,
                                 iRight1);
      vecLeft.push_back(iLeft1);
      vecRight.push_back(iRight1);
    }
    std::vector<int> vecLeftPosition;
    fnGetRelativePosition(vecLeft, vecLeftPosition);
    std::vector<int> vecRightPosition;
    fnGetRelativePosition(vecRight, vecRightPosition);

    vecStrLeftReorder.clear();
    vecStrRightReorder.clear();
    for (int i = 1; i < vecLeftPosition.size(); i++) {
      std::string strOutcome;
      fnGetOutcome(vecLeftPosition[i - 1], vecLeftPosition[i], strOutcome);
      vecStrLeftReorder.push_back(strOutcome);
      fnGetOutcome(vecRightPosition[i - 1], vecRightPosition[i], strOutcome);
      vecStrRightReorder.push_back(strOutcome);
    }
  }

  /*
   * features:
   * f1: (left_label, right_label, parent_label)
   * f2: (left_label, right_label, parent_label, other_right_sibling_label)
   * f3: (left_label, right_label, parent_label, other_left_sibling_label)
   * f4: (left_label, right_label, left_head_pos)
   * f5: (left_label, right_label, left_head_word)
   * f6: (left_label, right_label, right_head_pos)
   * f7: (left_label, right_label, right_head_word)
   * f8: (left_label, right_label, left_chunk_status)
   * f9: (left_label, right_label, right_chunk_status)
   * f10: (left_label, parent_label)
   * f11: (right_label, parent_label)
   *
   * f1: (left_role, right_role, predicate_term)
   * f2: (left_role, right_role, predicate_term, other_right_role)
   * f3: (left_role, right_role, predicate_term, other_left_role)
   * f4: (left_role, right_role, left_head_pos)
   * f5: (left_role, right_role, left_head_word)
   * f6: (left_role, right_role, left_syntactic_label)
   * f7: (left_role, right_role, right_head_pos)
   * f8: (left_role, right_role, right_head_word)
   * f8: (left_role, right_role, right_syntactic_label)
   * f8: (left_role, right_role, left_chunk_status)
   * f9: (left_role, right_role, right_chunk_status)
   * f10: (left_role, right_role, left_chunk_status)
   * f11: (left_role, right_role, right_chunk_status)
   * f12: (left_label, parent_label)
   * f13: (right_label, parent_label)
   */
  static void fnGenerateFeature(const SParsedTree *pTree,
                                const SPredicate *pPred,
                                const SPredicateItem *pPredItem, int iPos,
                                const std::string &strBlock1,
                                const std::string &strBlock2,
                                std::ostringstream &ostr) {
    SSRLItem *pSRLItem1 = pPredItem->vec_items_[iPos - 1];
    SSRLItem *pSRLItem2 = pPredItem->vec_items_[iPos];
    const STreeItem *pCon1 = pSRLItem1->tree_item_;
    const STreeItem *pCon2 = pSRLItem2->tree_item_;

    std::string left_role = pSRLItem1->role_;
    std::string right_role = pSRLItem2->role_;

    std::string predicate_term =
        pTree->m_vecTerminals[pPred->m_iPosition]->m_pszTerm;

    std::vector<std::string> vec_other_right_sibling;
    for (int i = iPos + 1; i < pPredItem->vec_items_.size(); i++)
      vec_other_right_sibling.push_back(
          std::string(pPredItem->vec_items_[i]->role_));
    if (vec_other_right_sibling.size() == 0)
      vec_other_right_sibling.push_back(std::string("NULL"));

    std::vector<std::string> vec_other_left_sibling;
    for (int i = 0; i < iPos - 1; i++)
      vec_other_right_sibling.push_back(
          std::string(pPredItem->vec_items_[i]->role_));
    if (vec_other_left_sibling.size() == 0)
      vec_other_left_sibling.push_back(std::string("NULL"));

    // generate features
    // f1
    ostr << "f1=" << left_role << "_" << right_role << "_" << predicate_term;
    ostr << "f1=" << left_role << "_" << right_role;

    // f2
    for (int i = 0; i < vec_other_right_sibling.size(); i++) {
      ostr << " f2=" << left_role << "_" << right_role << "_" << predicate_term
           << "_" << vec_other_right_sibling[i];
      ostr << " f2=" << left_role << "_" << right_role << "_"
           << vec_other_right_sibling[i];
    }
    // f3
    for (int i = 0; i < vec_other_left_sibling.size(); i++) {
      ostr << " f3=" << left_role << "_" << right_role << "_" << predicate_term
           << "_" << vec_other_left_sibling[i];
      ostr << " f3=" << left_role << "_" << right_role << "_"
           << vec_other_left_sibling[i];
    }
    // f4
    ostr << " f4=" << left_role << "_" << right_role << "_"
         << pTree->m_vecTerminals[pCon1->m_iHeadWord]->m_ptParent->m_pszTerm;
    // f5
    ostr << " f5=" << left_role << "_" << right_role << "_"
         << pTree->m_vecTerminals[pCon1->m_iHeadWord]->m_pszTerm;
    // f6
    ostr << " f6=" << left_role << "_" << right_role << "_" << pCon2->m_pszTerm;
    // f7
    ostr << " f7=" << left_role << "_" << right_role << "_"
         << pTree->m_vecTerminals[pCon2->m_iHeadWord]->m_ptParent->m_pszTerm;
    // f8
    ostr << " f8=" << left_role << "_" << right_role << "_"
         << pTree->m_vecTerminals[pCon2->m_iHeadWord]->m_pszTerm;
    // f9
    ostr << " f9=" << left_role << "_" << right_role << "_" << pCon2->m_pszTerm;
    // f10
    ostr << " f10=" << left_role << "_" << right_role << "_" << strBlock1;
    // f11
    ostr << " f11=" << left_role << "_" << right_role << "_" << strBlock2;
    // f12
    ostr << " f12=" << left_role << "_" << predicate_term;
    ostr << " f12=" << left_role;
    // f13
    ostr << " f13=" << right_role << "_" << predicate_term;
    ostr << " f13=" << right_role;
  }

 private:
  static void fnGetOutcome(int i1, int i2, std::string &strOutcome) {
    assert(i1 != i2);
    if (i1 < i2) {
      if (i2 > i1 + 1)
        strOutcome = std::string("DM");
      else
        strOutcome = std::string("M");
    } else {
      if (i1 > i2 + 1)
        strOutcome = std::string("DS");
      else
        strOutcome = std::string("S");
    }
  }

  static void fnGetRelativePosition(const std::vector<int> &vecLeft,
                                    std::vector<int> &vecPosition) {
    vecPosition.clear();

    std::vector<float> vec;
    for (int i = 0; i < vecLeft.size(); i++) {
      if (vecLeft[i] == -1) {
        if (i == 0)
          vec.push_back(-1);
        else
          vec.push_back(vecLeft[i - 1] + 0.1);
      } else
        vec.push_back(vecLeft[i]);
    }

    for (int i = 0; i < vecLeft.size(); i++) {
      int count = 0;

      for (int j = 0; j < vecLeft.size(); j++) {
        if (j == i) continue;
        if (vec[j] < vec[i]) {
          count++;
        } else if (vec[j] == vec[i] && j < i) {
          count++;
        }
      }
      vecPosition.push_back(count);
    }
  }
};
}  // namespace const_reorder

#endif  // _FF_CONST_REORDER_COMMON_H
