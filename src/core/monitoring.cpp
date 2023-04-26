/*******************************************************************************
 * Copyright (c) 2015, 2018, 2022 fortiss GmbH, Johannes Kepler University
 *  Primetals Technologies Austria GmbH
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *    Alois Zoitl
 *      - initial implementation and rework communication infrastructure
 *  Martin Melik Merkumians
 *    - adds functionality for W/CHAR
 *******************************************************************************/
#include <forte_config.h>
#include "monitoring.h"
#include "resource.h"
#include "device.h"
#include "ecet.h"
#include "utils/criticalregion.h"
#include "utils/string_utils.h"


using namespace forte::core;

CMonitoringHandler::CMonitoringHandler(CResource &paResource) :
    mTriggerEvent(nullptr, 0),
        mResource(paResource){
}

EMGMResponse CMonitoringHandler::executeMonitoringCommand(SManagementCMD &paCommand){
  EMGMResponse retVal = EMGMResponse::UnsupportedCmd;

  switch (paCommand.mCMD){
    case EMGMCommandType::MonitoringAddWatch:
      retVal = addWatch(paCommand.mFirstParam);
      break;
    case EMGMCommandType::MonitoringRemoveWatch:
      retVal = removeWatch(paCommand.mFirstParam);
      break;
    case EMGMCommandType::MonitoringReadWatches:
      retVal = readWatches(paCommand.mMonitorResponse);
      break;
    case EMGMCommandType::MonitoringForce:
      retVal = mResource.writeValue(paCommand.mFirstParam, paCommand.mAdditionalParams, true);
      break;
    case EMGMCommandType::MonitoringClearForce:
      retVal = clearForce(paCommand.mFirstParam);
      break;
    case EMGMCommandType::MonitoringTriggerEvent:
      retVal = triggerEvent(paCommand.mFirstParam);
      break;
    case EMGMCommandType::MonitoringResetEventCount:
      retVal = resetEventCount(paCommand.mFirstParam);
      break;
    default:
      break;
  }
  return retVal;
}

CFunctionBlock* CMonitoringHandler::getFB(forte::core::TNameIdentifier &paNameList){
  forte::core::TNameIdentifier::CIterator runner(paNameList.begin());

  CFunctionBlock *fb = mResource.getContainedFB(runner);
  if((nullptr != fb) && (!runner.isLastEntry())){
    ++runner;
    fb = fb->getFB(runner);
  }
  return fb;
}

EMGMResponse CMonitoringHandler::addWatch(forte::core::TNameIdentifier &paNameList){
  EMGMResponse eRetVal = EMGMResponse::NoSuchObject;

  CStringDictionary::TStringId portName = paNameList.back();
  paNameList.popBack();
  CFunctionBlock *fB = getFB(paNameList);

  if(nullptr != fB){
    SFBMonitoringEntry &fbMonitoringEntry(findOrCreateFBMonitoringEntry(fB, paNameList));

    CIEC_ANY *dataVal = fB->getVar(&portName, 1);
    if(nullptr != dataVal){
      addDataWatch(fbMonitoringEntry, portName, *dataVal);
      eRetVal = EMGMResponse::Ready;
    }
    else{
      TEventID eventId = fB->getEIID(portName);
      if(cg_nInvalidEventID != eventId){
        addEventWatch(fbMonitoringEntry, portName, fB->getEIMonitorData(eventId));
        eRetVal = EMGMResponse::Ready;
      }
      else{
        eventId = fB->getEOID(portName);
        if(cg_nInvalidEventID != eventId){
          addEventWatch(fbMonitoringEntry, portName, fB->getEOMonitorData(eventId));
          eRetVal = EMGMResponse::Ready;
        }
      }
    }
  }

  return eRetVal;
}

EMGMResponse CMonitoringHandler::removeWatch(forte::core::TNameIdentifier &paNameList){
  EMGMResponse eRetVal = EMGMResponse::NoSuchObject;

  CStringDictionary::TStringId portName = paNameList.back();
  paNameList.popBack();
  CFunctionBlock *fB = getFB(paNameList);
  if(nullptr != fB){
    TFBMonitoringList::Iterator itRefNode = mFBMonitoringList.end(); //needed for deleting the entry from the list
    TFBMonitoringList::Iterator itRunner = mFBMonitoringList.begin();

    while(itRunner != mFBMonitoringList.end()){
      if(itRunner->m_poFB == fB){
        SFBMonitoringEntry &poFBMonitoringEntry(*itRunner);

        if(removeDataWatch(poFBMonitoringEntry, portName) || removeEventWatch(poFBMonitoringEntry, portName)){ //if element is not watched, end search and return error

          if(poFBMonitoringEntry.m_lstWatchedDataPoints.isEmpty() && (poFBMonitoringEntry.m_lstWatchedEventPoints.isEmpty())){
            //no further values are monitored so remove the entry
            if(itRefNode == mFBMonitoringList.end()){
              //we have the first entry in the list
              mFBMonitoringList.popFront();
            }else{
              mFBMonitoringList.eraseAfter(itRefNode);
            }
          }
          eRetVal = EMGMResponse::Ready;
        }
        break;
      }

      itRefNode = itRunner;
      ++itRunner;
    }
  }
  return eRetVal;
}

EMGMResponse CMonitoringHandler::readWatches(CIEC_STRING &paResponse){
  paResponse.clear();
  if(nullptr == mResource.getResourcePtr()){
    //we are in the device
    for(CFBContainer::TFunctionBlockList::Iterator itRunner = mResource.getFBList().begin();
        itRunner != mResource.getFBList().end();
        ++itRunner){
      ((CResource*) (*itRunner))->getMonitoringHandler().readResourceWatches(paResponse);
    }
  }
  else{
    //we are within a resource
    readResourceWatches(paResponse);
  }

  return EMGMResponse::Ready;
}

EMGMResponse CMonitoringHandler::clearForce(forte::core::TNameIdentifier &paNameList){
  EMGMResponse eRetVal = EMGMResponse::NoSuchObject;
  CStringDictionary::TStringId portName = paNameList.back();
  paNameList.popBack();
  CFunctionBlock *fB = getFB(paNameList);

  if(nullptr != fB){
    CIEC_ANY *poDataVal = fB->getVar(&portName, 1);
    if(nullptr != poDataVal){
      poDataVal->setForced(false);
      eRetVal = EMGMResponse::Ready;
    }
  }
  return eRetVal;
}

EMGMResponse CMonitoringHandler::triggerEvent(forte::core::TNameIdentifier &paNameList){
  EMGMResponse eRetVal = EMGMResponse::NoSuchObject;
  CStringDictionary::TStringId portName = paNameList.back();
  paNameList.popBack();
  CFunctionBlock *fB = getFB(paNameList);
  if(nullptr != fB){
    TEventID eventId = fB->getEIID(portName);
    if(cg_nInvalidEventID != eventId){
      //only a single event can be triggered simultaneously (until it is executed by ecet!)
      //otherwise the triggerEvent structure needs to be moved to another place!
      mTriggerEvent.mFB = fB;
      mTriggerEvent.mPortId = eventId;
      mResource.getResourceEventExecution()->startEventChain(&mTriggerEvent);
      eRetVal = EMGMResponse::Ready;
    }
    else{
      eventId = fB->getEOID(portName);
      if(cg_nInvalidEventID != eventId){
        fB->m_poInvokingExecEnv = mResource.getResourceEventExecution();
        fB->sendOutputEvent(eventId);
        mResource.getResourceEventExecution()->resumeSelfSuspend();
        eRetVal = EMGMResponse::Ready;
      }
    }

  }
  return eRetVal;
}

EMGMResponse CMonitoringHandler::resetEventCount(forte::core::TNameIdentifier &paNameList){
  EMGMResponse eRetVal = EMGMResponse::NoSuchObject;
  CStringDictionary::TStringId portName = paNameList.back();
  paNameList.popBack();
  CFunctionBlock *fB = getFB(paNameList);
  if(nullptr != fB){
    TEventID eventId = fB->getEIID(portName);
    TForteUInt32 *eventMonitorData = nullptr;

    if(cg_nInvalidEventID != eventId){
      eventMonitorData = &fB->getEIMonitorData(eventId);
    }
    else{
      eventId = fB->getEOID(portName);
      if(cg_nInvalidEventID != eventId){
        eventMonitorData = &fB->getEOMonitorData(eventId);
      }
    }
    if(nullptr != eventMonitorData){
      CCriticalRegion criticalRegion(fB->getResource().m_oResDataConSync);
      *eventMonitorData = 0;
      eRetVal = EMGMResponse::Ready;
    }
  }
  return eRetVal;
}

CMonitoringHandler::SFBMonitoringEntry &CMonitoringHandler::findOrCreateFBMonitoringEntry(
    CFunctionBlock *paFB, forte::core::TNameIdentifier &paNameList){
  for(TFBMonitoringList::Iterator itRunner = mFBMonitoringList.begin();
      itRunner != mFBMonitoringList.end(); ++itRunner){
    if(itRunner->m_poFB == paFB){
      return *itRunner;
    }
  }

  mFBMonitoringList.pushBack(SFBMonitoringEntry());
  TFBMonitoringList::Iterator itLastEntry(mFBMonitoringList.back());
  itLastEntry->m_poFB = paFB;
  createFullFBName(itLastEntry->mFullFBName, paNameList);
  return *itLastEntry;
}

void CMonitoringHandler::addDataWatch(SFBMonitoringEntry &paFBMonitoringEntry,
    CStringDictionary::TStringId paPortId, CIEC_ANY &paDataVal){
  for(TDataWatchList::Iterator itRunner = paFBMonitoringEntry.m_lstWatchedDataPoints.begin();
      itRunner != paFBMonitoringEntry.m_lstWatchedDataPoints.end(); ++itRunner){
    if(itRunner->mPortId == paPortId){
      //the data point is already in the watch list
      return;
    }
  }
  paFBMonitoringEntry.m_lstWatchedDataPoints.pushBack(SDataWatchEntry(paPortId, paDataVal));
}

bool CMonitoringHandler::removeDataWatch(SFBMonitoringEntry &paFBMonitoringEntry,
    CStringDictionary::TStringId paPortId){
  bool bRetVal = false;

  TDataWatchList::Iterator itRunner = paFBMonitoringEntry.m_lstWatchedDataPoints.begin();
  TDataWatchList::Iterator itRefNode = paFBMonitoringEntry.m_lstWatchedDataPoints.end();

  while(itRunner != paFBMonitoringEntry.m_lstWatchedDataPoints.end()){
    if(itRunner->mPortId == paPortId){
      if(itRefNode == paFBMonitoringEntry.m_lstWatchedDataPoints.end()){
        //we have the first entry in the list
        paFBMonitoringEntry.m_lstWatchedDataPoints.popFront();
      }
      else{
        paFBMonitoringEntry.m_lstWatchedDataPoints.eraseAfter(itRefNode);
      }
      bRetVal = true;
      break;
    }

    itRefNode = itRunner;
    ++itRunner;
  }

  return bRetVal;
}

void CMonitoringHandler::addEventWatch(SFBMonitoringEntry &paFBMonitoringEntry, CStringDictionary::TStringId paPortId, TForteUInt32 &paEventData){
  for(TEventWatchList::Iterator itRunner = paFBMonitoringEntry.m_lstWatchedEventPoints.begin();
      itRunner != paFBMonitoringEntry.m_lstWatchedEventPoints.end(); ++itRunner){
    if(itRunner->mPortId == paPortId){
      //the event point is already in the watch list
      return;
    }
  }
  paFBMonitoringEntry.m_lstWatchedEventPoints.pushBack(SEventWatchEntry(paPortId, paEventData));
}

bool CMonitoringHandler::removeEventWatch(SFBMonitoringEntry &paFBMonitoringEntry, CStringDictionary::TStringId paPortId){
  bool bRetVal = false;

  TEventWatchList::Iterator itRunner = paFBMonitoringEntry.m_lstWatchedEventPoints.begin();
  TEventWatchList::Iterator itRefNode = paFBMonitoringEntry.m_lstWatchedEventPoints.end();

  while(itRunner != paFBMonitoringEntry.m_lstWatchedEventPoints.end()){
    if(itRunner->mPortId == paPortId){
      if(itRefNode == paFBMonitoringEntry.m_lstWatchedEventPoints.end()){
        //we have the first entry in the list
        paFBMonitoringEntry.m_lstWatchedEventPoints.popFront();
      }
      else{
        paFBMonitoringEntry.m_lstWatchedEventPoints.eraseAfter(itRefNode);
      }
      bRetVal = true;
      break;
    }

    itRefNode = itRunner;
    ++itRunner;
  }

  return bRetVal;
}

void CMonitoringHandler::readResourceWatches(CIEC_STRING &paResponse){
  if(!mFBMonitoringList.isEmpty()){
    paResponse.append("<Resource name=\"");
    paResponse.append(mResource.getInstanceName());
    paResponse.append("\">");

    updateMonitringData();

    for(TFBMonitoringList::Iterator itRunner = mFBMonitoringList.begin(); itRunner != mFBMonitoringList.end(); ++itRunner){
      paResponse.append("<FB name=\"");
      paResponse.append(itRunner->mFullFBName.getValue());
      paResponse.append("\">");

      //add the data watches
      for(TDataWatchList::Iterator itDataRunner = itRunner->m_lstWatchedDataPoints.begin(); itDataRunner != itRunner->m_lstWatchedDataPoints.end(); ++itDataRunner){
        appendDataWatch(paResponse, *itDataRunner);
      }

      //add the event watches
      for(TEventWatchList::Iterator itEventRunner = itRunner->m_lstWatchedEventPoints.begin(); itEventRunner != itRunner->m_lstWatchedEventPoints.end(); ++itEventRunner){
        appendEventWatch(paResponse, *itEventRunner);
      }

      paResponse.append("</FB>");
    }
    paResponse.append("</Resource>");
  }
}


void CMonitoringHandler::updateMonitringData(){
  //update the monitoring data buffer to keep the critical region as short as possible
  CCriticalRegion criticalRegion(mResource.m_oResDataConSync);
  for(TFBMonitoringList::Iterator itRunner = mFBMonitoringList.begin(); itRunner != mFBMonitoringList.end(); ++itRunner){

    for(TDataWatchList::Iterator itDataRunner = itRunner->m_lstWatchedDataPoints.begin(); itDataRunner != itRunner->m_lstWatchedDataPoints.end(); ++itDataRunner){
      itDataRunner->mDataBuffer->setValue(itDataRunner->mDataValueRef);
      itDataRunner->mDataBuffer->setForced(itDataRunner->mDataValueRef.isForced());
    }

    for(TEventWatchList::Iterator itEventRunner = itRunner->m_lstWatchedEventPoints.begin(); itEventRunner != itRunner->m_lstWatchedEventPoints.end(); ++itEventRunner){
      itEventRunner->mEventDataBuf = itEventRunner->mEventDataRef;
    }
  }
}


void CMonitoringHandler::appendDataWatch(CIEC_STRING &paResponse,
    SDataWatchEntry &paDataWatchEntry){
  appendPortTag(paResponse, paDataWatchEntry.mPortId);
  paResponse.append("<Data value=\"");
  size_t bufferSize = paDataWatchEntry.mDataBuffer->getToStringBufferSize() + getExtraSizeForEscapedChars(*paDataWatchEntry.mDataBuffer);
  char* acDataValue = new char [bufferSize]; //TODO try to directly use the response string instead
  int consumedBytes = paDataWatchEntry.mDataBuffer->toString(acDataValue, bufferSize);
  if(consumedBytes > 0 && static_cast<size_t>(consumedBytes) < bufferSize) {
    switch (paDataWatchEntry.mDataBuffer->getDataTypeID()) {
      case CIEC_ANY::e_ANY:
      case CIEC_ANY::e_WSTRING:
      case CIEC_ANY::e_STRING:
      case CIEC_ANY::e_CHAR:
      case CIEC_ANY::e_WCHAR:
      case CIEC_ANY::e_ARRAY:
      case CIEC_ANY::e_STRUCT:
        consumedBytes += static_cast<int>(forte::core::util::transformNonEscapedToEscapedXMLText(acDataValue));
        break;
      default:
        break;
    }
    acDataValue[consumedBytes] = '\0';
    paResponse.append(acDataValue);
  }
  paResponse.append("\" forced=\"");
  paResponse.append((paDataWatchEntry.mDataBuffer->isForced()) ? "true" : "false");
  paResponse.append("\"/></Port>");
  delete [] acDataValue;
}

size_t CMonitoringHandler::getExtraSizeForEscapedChars(const CIEC_ANY& paDataValue){
  size_t retVal = 0;

  switch(paDataValue.getDataTypeID()){
    case CIEC_ANY::e_ANY:
      retVal = getExtraSizeForEscapedChars(paDataValue.unwrap());
      break;
    case CIEC_ANY::e_WSTRING:
    case CIEC_ANY::e_STRING:
      retVal = forte::core::util::getExtraSizeForXMLEscapedChars(static_cast<const CIEC_WSTRING&>(paDataValue).getValue()) + 10; //for opening and closing quotes or apos
      break;
    case CIEC_ANY::e_CHAR:
      retVal = 5 + 5 + 5; // Both outer quotes and symbol gets evetually replaced
      break;
    case CIEC_ANY::e_WCHAR:
      retVal = 5 + 5 + 5; //Both outer quotes and symbol gets evetually replaced
    break;
    case CIEC_ANY::e_ARRAY:
      retVal = getExtraSizeForEscapedCharsArray(static_cast<const CIEC_ARRAY_TYPELIB &>(paDataValue));
      break;
    case CIEC_ANY::e_STRUCT:
      retVal = getExtraSizeForEscapedCharsStruct(static_cast<const CIEC_STRUCT&>(paDataValue));
      break;
    default:
      break;
  }

  return retVal;
}

size_t CMonitoringHandler::getExtraSizeForEscapedCharsArray(const CIEC_ARRAY_TYPELIB &paDataValue)
{
  size_t retVal = 0;

  switch(paDataValue[0].getDataTypeID()){
    case CIEC_ANY::e_WSTRING:
    case CIEC_ANY::e_STRING:
      for(size_t i = 0; i < paDataValue.size(); i++) {
        retVal += forte::core::util::getExtraSizeForXMLEscapedChars(static_cast<const CIEC_WSTRING&>(paDataValue[static_cast<TForteUInt16>(i)]).getValue()) + 10; //for opening and closing quotes or apos
      }
      break;
    case CIEC_ANY::e_STRUCT:
      for(size_t i = 0; i < paDataValue.size(); i++) {
        retVal += getExtraSizeForEscapedCharsStruct(static_cast<const CIEC_STRUCT&>(paDataValue[static_cast<TForteUInt16>(i)]));
      }
      break;
    default:
      break;
  }

  return retVal;
}

size_t CMonitoringHandler::getExtraSizeForEscapedCharsStruct(const CIEC_STRUCT& paDataValue) {
  size_t retVal = 0;

  const CIEC_ANY* members = paDataValue.getMembers();
  for(size_t i = 0; i < paDataValue.getStructSize(); i++) {
    switch(members[i].getDataTypeID()){
      case CIEC_ANY::e_WSTRING:
      case CIEC_ANY::e_STRING:
        retVal += forte::core::util::getExtraSizeForXMLEscapedChars(static_cast<const CIEC_WSTRING&>(members[i]).getValue()) + 10; //for opening and closing quotes or apos
        break;
      case CIEC_ANY::e_ARRAY:
        retVal += getExtraSizeForEscapedCharsArray(static_cast<const CIEC_ARRAY_TYPELIB &>(members[i]));
        break;
      case CIEC_ANY::e_STRUCT:
        retVal += getExtraSizeForEscapedCharsStruct(static_cast<const CIEC_STRUCT&>(members[i]));
        break;
      default:
        break;
    }
  }

  return retVal;
}

void CMonitoringHandler::appendPortTag(CIEC_STRING &paResponse,
    CStringDictionary::TStringId paPortId){
  paResponse.append("<Port name=\"");
  paResponse.append(CStringDictionary::getInstance().get(paPortId));
  paResponse.append("\">");
}

void CMonitoringHandler::appendEventWatch(CIEC_STRING &paResponse, SEventWatchEntry &paEventWatchEntry){
  appendPortTag(paResponse, paEventWatchEntry.mPortId);

  CIEC_UDINT udint(paEventWatchEntry.mEventDataBuf);
  CIEC_ULINT ulint(mResource.getDevice().getTimer().getForteTime());

  paResponse.append("<Data value=\"");
  char buf[21]; // the bigest number in an ulint is 18446744073709551616, TODO directly use pa_roResponse
  udint.toString(buf, sizeof(buf));
  paResponse.append(buf);
  paResponse.append("\" time=\"");
  ulint.toString(buf, sizeof(buf));
  paResponse.append(buf);
  paResponse.append("\"/>\n</Port>");
}

void CMonitoringHandler::createFullFBName(CIEC_STRING &paFullName, forte::core::TNameIdentifier &paNameList){
  for(forte::core::TNameIdentifier::CIterator runner(paNameList.begin()); runner != paNameList.end(); ++runner){
    paFullName.append(CStringDictionary::getInstance().get(*runner));
    if(!runner.isLastEntry()){
      paFullName.append(".");
    }
  }
}

