/*******************************************************************************
 * Copyright (c) 2012, 2023 Profactor GmbH, ACIN, fortiss Gmbh
 *                          Johannes Kepler University
 *                          Martin Erich Jobst
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0.
 *
 * SPDX-License-Identifier: EPL-2.0
 *
 * Contributors:
 *   Matthias Plasch, Monika Wenger, Alois Zoitl
 *   - initial API and implementation and/or initial documentation
 *    Alois Zoitl - introduced new CGenFB class for better handling generic FBs
 *    Martin Jobst - add generic readInputData and writeOutputData
 *******************************************************************************/
#ifndef _GEN_F_MUX_H_
#define _GEN_F_MUX_H_

#include <genfb.h>

class GEN_F_MUX : public CGenFunctionBlock<CFunctionBlock> {
  DECLARE_GENERIC_FIRMWARE_FB(GEN_F_MUX)

  private:
    //we know for sure that there is one output event
    static const CStringDictionary::TStringId scm_anEventOutputNames[];

    static const TEventID scm_nEventEOID = 0;

    CStringDictionary::TStringId *m_anEventInputNames;
    CStringDictionary::TStringId *m_anDataOutputNames;
    CStringDictionary::TStringId *m_anDataInputNames;
    CStringDictionary::TStringId *m_anDataOutputTypeIds;
    CStringDictionary::TStringId *m_anDataInputTypeIds;

    //self-defined members
    size_t mEInputs;
    size_t mEOutputs;
    size_t mDInputs;
    size_t mDOutputs;

    void executeEvent(TEventID paEIID) override;

    void readInputData(TEventID paEI) override;
    void writeOutputData(TEventID paEO) override;

    bool createInterfaceSpec(const char *paConfigString, SFBInterfaceSpec &paInterfaceSpec) override;

    GEN_F_MUX(const CStringDictionary::TStringId paInstanceNameId, CResource *paSrcRes);
    ~GEN_F_MUX() override;
};
#endif //_GEN_F_MUX_H_
