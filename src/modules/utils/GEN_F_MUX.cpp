/*******************************************************************************
 * Copyright (c) 2012 - 2013 Profactor GmbH, ACIN
 *                      2018 Johannes Kepler University
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *
 * Contributors:
 *   Matthias Plasch, Alois Zoitl
 *   - initial API and implementation and/or initial documentation
 *    Alois Zoitl - introduced new CGenFB class for better handling generic FBs
 *******************************************************************************/
#include "GEN_F_MUX.h"
#ifdef FORTE_ENABLE_GENERATED_SOURCE_CPP
#include "GEN_F_MUX_gen.cpp"
#endif
#include <forte_printer.h>
#include <ctype.h>
#include <stdio.h>

DEFINE_GENERIC_FIRMWARE_FB(GEN_F_MUX, g_nStringIdGEN_F_MUX);

const CStringDictionary::TStringId GEN_F_MUX::scm_anEventOutputNames[] = { g_nStringIdEO };

GEN_F_MUX::GEN_F_MUX(const CStringDictionary::TStringId paInstanceNameId, CResource *paSrcRes) :
    CGenFunctionBlock<CFunctionBlock>(paSrcRes, paInstanceNameId),
  m_anEventInputNames(0),
  m_anDataOutputNames(0),
  m_anDataInputNames(0),
  m_anDataOutputTypeIds(0),
  m_anDataInputTypeIds(0),
  m_anEIWithIndexes(0),
  m_anEIWith(0),
  m_anEOWithIndexes(0),
  m_anEOWith(0),
  m_nEInputs(0),
  m_nEOutputs(0),
  m_nDInputs(0),
  m_nDOutputs(0){
}

GEN_F_MUX::~GEN_F_MUX(){
  delete[] m_anEventInputNames;
  delete[] m_anDataInputNames;
  delete[] m_anDataInputTypeIds;
  delete[] m_anDataOutputNames;
  delete[] m_anDataOutputTypeIds;
  delete[] m_anEIWith;
  delete[] m_anEIWithIndexes;
  delete[] m_anEOWith;
  delete[] m_anEOWithIndexes;
}

void GEN_F_MUX::executeEvent(int paEIID){

  if(-1 < paEIID && static_cast<unsigned int>(paEIID) < m_nEInputs ){

    int startIndex = paEIID * m_nDOutputs;
    bool status = true;

    for(int input_index = startIndex, output_index = 2; input_index < startIndex + static_cast<int>(m_nDOutputs); input_index++, output_index++){

      CIEC_ANY *p_dataInput = getDI(input_index);
      CIEC_ANY *p_dataOutput = getDO(output_index);

      // check whether datatypes match
      if(p_dataInput != 0 && p_dataOutput != 0 && p_dataInput->getDataTypeID() == p_dataOutput->getDataTypeID()){

        p_dataOutput->setValue(*p_dataInput);
        status = true && status;
      }
      else{
        status = false;
      }
    }

    if(status == true){
      *static_cast<CIEC_BOOL*>(getDO(0)) = true;
      *static_cast<CIEC_WSTRING *>(getDO(1)) = "OK";
    }
    else{
      *static_cast<CIEC_BOOL*>(getDO(0)) = false;
      *static_cast<CIEC_WSTRING *>(getDO(1)) = "Datatype ERROR";
    }

    sendOutputEvent(0);
  }
}

bool GEN_F_MUX::createInterfaceSpec(const char *paConfigString, SFBInterfaceSpec &paInterfaceSpec) {
  int index = 0;
  char *paramEI = 0;
  char *paramDO = 0;
  char baseName[cg_nIdentifierLength];

  TIdentifier typeIdString;
  size_t inlength;

  memcpy(typeIdString, paConfigString, cg_nIdentifierLength);

  typeIdString[cg_nIdentifierLength] = '\0'; //make a string

  inlength = strlen(typeIdString);

  for(index = 0; index < (int) inlength - 1; index++){

    if(typeIdString[index] == '_'){

      //make sure that there is no invalid index
      if(index - 1 >= 0){
        memcpy(&baseName[0], &typeIdString[index - 1], 5); // F_MUX ... prefix has 5 characters
        baseName[5] = '\0';

        //check if the prefix F_MUX exists; in this case, the next "_" separates the parameters
        if(strcmp(baseName, "F_MUX") == 0){
          continue;
        }
      }
      else{
        //error on creating the FB; this would mean that the Typename starts with "_"
        return false;
      }

      paramEI = paramDO = &(typeIdString[index + 1]);
      break;
    }
  }
  if(paramDO != 0){

    for(index = index + 1; index < (int) inlength - 1; index++){

      if(typeIdString[index] == '_'){

        typeIdString[index] = '\0';
        paramDO = &(typeIdString[index + 1]);
        break;
      }
    }
  }

  if(paramDO == 0){
    return false;
  }
  else{
    //set the data and event port numbers
    m_nEInputs = static_cast<unsigned int>(forte::core::util::strtoul(paramEI,0,10));
    m_nDOutputs = static_cast<unsigned int>(forte::core::util::strtoul(paramDO,0,10));
    m_nEOutputs = 1;
    m_nDInputs = m_nEInputs * m_nDOutputs;

    DEVLOG_DEBUG("EIs: %d; DIs: %d; EOs: %d; DOs: %d \n", m_nEInputs, m_nDInputs, m_nEOutputs, m_nDOutputs);

    //return with error if there are not enough event inputs (use common merge FB instead!!)
    if(m_nEInputs < 2){
      DEVLOG_ERROR("At least 2 Event Inputs need to be set\n");
      return 0;
    }
  }

  //now the number of needed eventInputs and dataOutputs are available in the integer array
  //create the eventInputs
  if(m_nEInputs < CFunctionBlock::scm_nMaxInterfaceEvents && m_nDInputs < CFunctionBlock::scm_nMaxInterfaceEvents){
    //create the eventInputs
    m_anEventInputNames = new CStringDictionary::TStringId[m_nEInputs];

    generateGenericInterfacePointNameArray("EI", m_anEventInputNames,  m_nEInputs);

    //create the data inputs
    m_anDataInputNames = new CStringDictionary::TStringId[m_nDInputs];
    m_anDataInputTypeIds = new CStringDictionary::TStringId[m_nDInputs];
    char diNames[cg_nIdentifierLength] = { "IN_" };
    int di_posIndex = 0;
    for(unsigned int ei = 0; ei < m_nEInputs; ei = ei + 1){

      for(unsigned int di = 0; di < m_nDOutputs; di = di + 1){
        forte_snprintf(&(diNames[3]), 11 - 3, "%u_%u", ei + 1, di + 1);
        di_posIndex = ei * m_nDOutputs + di;
        m_anDataInputNames[di_posIndex] = CStringDictionary::getInstance().insert(diNames);
        m_anDataInputTypeIds[di_posIndex] = g_nStringIdANY;
      }
    }

    //create the data outputs
    m_anDataOutputNames = new CStringDictionary::TStringId[m_nDOutputs + 2];
    m_anDataOutputTypeIds = new CStringDictionary::TStringId[m_nDOutputs + 2];

    //data outputs for status and QO
    m_anDataOutputNames[0] = CStringDictionary::getInstance().insert("QO");
    m_anDataOutputTypeIds[0] = g_nStringIdBOOL;
    m_anDataOutputNames[1] = CStringDictionary::getInstance().insert("STATUS");
    m_anDataOutputTypeIds[1] = g_nStringIdWSTRING;

    generateGenericDataPointArrays("OUT_", &(m_anDataOutputTypeIds[2]), &(m_anDataOutputNames[2]), m_nDOutputs);

    //now create the WITH constructs...
    //first the With-Indexes Events
    m_anEIWithIndexes = new TForteInt16[m_nEInputs];
    m_anEOWithIndexes = new TForteInt16[2]; //contains terminating -1 value

    for(unsigned int ei_index = 0; ei_index < m_nEInputs; ei_index = ei_index + 1){
      if(ei_index == 0){
        m_anEIWithIndexes[ei_index] = 0;
      }
      else{
        m_anEIWithIndexes[ei_index] = static_cast<TForteInt16>(ei_index * (m_nDOutputs + 1));
      }
    }

    m_anEOWithIndexes[0] = 0;
    m_anEOWithIndexes[1] = -1;

    //second the With-Indexes for the data variables
    m_anEIWith = new TDataIOID[m_nDInputs + m_nEInputs]; //for inputs per event + '255' separators between withs
    m_anEOWith = new TDataIOID[m_nDOutputs + 2 + 1]; //for outputs only one '255' separator since one output event is needed + 2 for QO and STATUS

    //in-withs
    int withListIndex = 0;
    int dataIndex = 0;
    for(unsigned int in_block = 0; in_block < m_nEInputs; in_block = in_block + 1){

      for(unsigned int in_with = 0; in_with < m_nDOutputs; in_with = in_with + 1){
        m_anEIWith[withListIndex] = static_cast<TDataIOID>(dataIndex);
        withListIndex++;
        dataIndex++;
      }

      //set '255' separator
      m_anEIWith[withListIndex] = scmWithListDelimiter;
      withListIndex++;
    }

    //out-withs
    m_anEOWith[0] = 0; //for QO and STATUS
    m_anEOWith[1] = 1;

    for(unsigned int out_with = 2; out_with < m_nDOutputs + 2; out_with = out_with + 1){
      m_anEOWith[out_with] = static_cast<TForteUInt8>(out_with);
    }
    //set '255' separator
    m_anEOWith[m_nDOutputs + 2] = scmWithListDelimiter;

    //create the interface Specification
    paInterfaceSpec.m_nNumEIs = static_cast<TForteUInt8>(m_nEInputs);
    paInterfaceSpec.m_aunEINames = m_anEventInputNames;
    paInterfaceSpec.m_anEIWith = m_anEIWith;
    paInterfaceSpec.m_anEIWithIndexes = m_anEIWithIndexes;
    paInterfaceSpec.m_nNumEOs = static_cast<TForteUInt8>(m_nEOutputs);
    paInterfaceSpec.m_aunEONames = scm_anEventOutputNames;
    paInterfaceSpec.m_anEOWith = m_anEOWith;
    paInterfaceSpec.m_anEOWithIndexes = m_anEOWithIndexes;
    paInterfaceSpec.m_nNumDIs = static_cast<TForteUInt8>(m_nDInputs);
    paInterfaceSpec.m_aunDINames = m_anDataInputNames;
    paInterfaceSpec.m_aunDIDataTypeNames = m_anDataInputTypeIds;
    paInterfaceSpec.m_nNumDOs = static_cast<TForteUInt8>(m_nDOutputs + 2);
    paInterfaceSpec.m_aunDONames = m_anDataOutputNames;
    paInterfaceSpec.m_aunDODataTypeNames = m_anDataOutputTypeIds;
    return true;
  }
  return false;
}
