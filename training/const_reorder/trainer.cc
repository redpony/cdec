#include "trainer.h"

#include "utils/maxent.h"

Tsuruoka_Maxent_Trainer::Tsuruoka_Maxent_Trainer()
    : const_reorder::Tsuruoka_Maxent(NULL) {}

void Tsuruoka_Maxent_Trainer::fnTrain(const char* pszInstanceFName,
                                      const char* pszAlgorithm,
                                      const char* pszModelFName) {
  assert(strcmp(pszAlgorithm, "l1") == 0 || strcmp(pszAlgorithm, "l2") == 0 ||
         strcmp(pszAlgorithm, "sgd") == 0 || strcmp(pszAlgorithm, "SGD") == 0);
  FILE* fpIn = fopen(pszInstanceFName, "r");

  maxent::ME_Model* pModel = new maxent::ME_Model();

  char* pszLine = new char[100001];
  int iNumInstances = 0;
  int iLen;
  while (!feof(fpIn)) {
    pszLine[0] = '\0';
    fgets(pszLine, 20000, fpIn);
    if (strlen(pszLine) == 0) {
      continue;
    }

    iLen = strlen(pszLine);
    while (iLen > 0 && pszLine[iLen - 1] > 0 && pszLine[iLen - 1] < 33) {
      pszLine[iLen - 1] = '\0';
      iLen--;
    }

    iNumInstances++;

    maxent::ME_Sample* pmes = new maxent::ME_Sample();

    char* p = strrchr(pszLine, ' ');
    assert(p != NULL);
    p[0] = '\0';
    p++;
    std::vector<std::string> vecContext;
    SplitOnWhitespace(std::string(pszLine), &vecContext);

    pmes->label = std::string(p);
    for (size_t i = 0; i < vecContext.size(); i++)
      pmes->add_feature(vecContext[i]);
    pModel->add_training_sample((*pmes));
    if (iNumInstances % 100000 == 0)
      fprintf(stdout, "......Reading #Instances: %1d\n", iNumInstances);
    delete pmes;
  }
  fprintf(stdout, "......Reading #Instances: %1d\n", iNumInstances);
  fclose(fpIn);

  if (strcmp(pszAlgorithm, "l1") == 0)
    pModel->use_l1_regularizer(1.0);
  else if (strcmp(pszAlgorithm, "l2") == 0)
    pModel->use_l2_regularizer(1.0);
  else
    pModel->use_SGD();

  pModel->train();
  pModel->save_to_file(pszModelFName);

  delete pModel;
  fprintf(stdout, "......Finished Training\n");
  fprintf(stdout, "......Model saved as %s\n", pszModelFName);
  delete[] pszLine;
}
