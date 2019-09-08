/*
 * _SortingImg.cpp
 *
 *  Created on: May 28, 2019
 *      Author: yankai
 */

#include "_SortingImgServer.h"

namespace kai
{

_SortingImgServer::_SortingImgServer()
{
	m_pOL = NULL;
	m_cSpeed = 0.0;
	m_cLen = 2.0;
	m_minOverlap = 0.8;
	m_ID = 0;
	m_timeOutVerify = USEC_1SEC * 3;
	m_timeOutShow = USEC_1SEC * 1;
	m_tIntSend = 100000;
	m_tLastSent = 0;
	m_bCOO = false;
	m_COO.init();
}

_SortingImgServer::~_SortingImgServer()
{
}

bool _SortingImgServer::init(void *pKiss)
{
	IF_F(!this->_DetectorBase::init(pKiss));
	Kiss *pK = (Kiss*) pKiss;

	pK->v("cSpeed", &m_cSpeed);
	pK->v("cLen", &m_cLen);
	pK->v("minOverlap", &m_minOverlap);
	pK->v("timeOutVerify", &m_timeOutVerify);
	pK->v("timeOutShow", &m_timeOutShow);

	m_nClass = m_pDB->m_nClass;

	string iName;

	iName = "";
	F_ERROR_F(pK->v("_OKlink", &iName));
	m_pOL = (_OKlink*) (pK->root()->getChildInst(iName));
	IF_Fl(!m_pOL, iName + " not found");

	return true;
}

bool _SortingImgServer::start(void)
{
	m_bThreadON = true;
	int retCode = pthread_create(&m_threadID, 0, getUpdateThread, this);
	if (retCode != 0)
	{
		m_bThreadON = false;
		return false;
	}

	return true;
}

int _SortingImgServer::check(void)
{
	NULL__(m_pOL, -1);
	NULL__(m_pVision, -1);

	return 0;
}

void _SortingImgServer::update(void)
{
	while (m_bThreadON)
	{
		this->autoFPSfrom();

		updateImg();
		updateObj();

		if(m_bCOO)
		{
			if(m_tStamp - m_tLastSent > m_tIntSend)
			{
				m_pOL->sendBB(m_COO.m_id, m_COO.m_topClass, m_COO.m_bb);
				m_tLastSent = m_tStamp;
			}
		}

		if (m_bGoSleep)
		{
			m_pPrev->reset();
		}

		this->autoFPSto();
	}
}

void _SortingImgServer::updateImg(void)
{
	IF_(check() < 0);

	//update existing target positions
	float spd = m_cSpeed * ((float) m_dTime) * 1e-6;
	int i = 0;
	OBJECT *pO;
	while ((pO = at(i++)))
	{
		pO->m_bb.y += spd;
		pO->m_bb.w += spd;
		IF_CONT(pO->m_bb.midY() > m_cLen);

		add(pO);
	}

	//get new targets and compare to existing ones
	i = 0;
	while ((pO = m_pDB->at(i++)))
	{
		IF_CONT(pO->m_topClass < 0);

		int j = 0;
		OBJECT *pT;
		while ((pT = m_pNext->at(j++)))
		{
			IF_CONT(nIoU(pT->m_bb, pO->m_bb) < m_minOverlap);

			if (pT->m_topProb < pO->m_topProb)
			{
				pT->m_topClass = pO->m_topClass;
				pT->m_topProb = pO->m_topProb;
			}
			break;
		}
		IF_CONT(pT);

		pO->m_id = m_ID++;
		pO->m_tStamp = m_tStamp;
		pO->m_bVerified = false;
		pO->setImg(*m_pVision->BGR()->m());
		add(pO);
	}

	uint64_t dT = m_tStamp - m_COO.m_tStamp;
	IF_(m_bCOO &&
			dT < m_timeOutVerify &&
			!m_COO.m_bVerified);

	//update the current operating object which is verified
	if(m_bCOO && m_COO.m_bVerified)
	{
		i = 0;
		while ((pO = m_pNext->at(i++)))
		{
			IF_CONT(m_COO.m_id != pO->m_id);

			pO->m_topClass = m_COO.m_topClass;
			pO->m_bb = m_COO.m_bb;
			pO->m_bVerified = m_COO.m_bVerified;
			break;
		}
	}

	//update the next operating object to be verified
	i = 0;
	m_bCOO = false;
	while ((pO = m_pNext->at(i++)))
	{
		IF_CONT(pO->m_bVerified);
		IF_CONT(m_tStamp - pO->m_tStamp > m_timeOutShow);

		m_COO = *pO;
		m_bCOO = true;
		break;
	}
}

void _SortingImgServer::handleCMD(uint8_t* pCMD)
{
	NULL_(pCMD);
	IF_(pCMD[1] != OKLINK_BB);

	int id = unpack_uint32(&pCMD[3], false);

	int i = 0;
	OBJECT *pO;
	while ((pO = at(i++)))
	{
		IF_CONT(pO->m_id != id);

		pO->m_topClass = unpack_int16(&pCMD[7], false);
		pO->m_bb.x = ((float)unpack_uint16(&pCMD[9], false))*0.001;
		pO->m_bb.y = ((float)unpack_uint16(&pCMD[11], false))*0.001;
		pO->m_bb.z = ((float)unpack_uint16(&pCMD[13], false))*0.001;
		pO->m_bb.w = ((float)unpack_uint16(&pCMD[15], false))*0.001;
		pO->m_bVerified = true;
	}
}

bool _SortingImgServer::draw(void)
{
	IF_F(!this->_DetectorBase::draw());
	Window *pWin = (Window*) this->m_pWindow;
	Mat *pMat = pWin->getFrame()->m();
	IF_F(pMat->empty());

	if(m_bCOO)
	{
		m_COO.m_mImg.copyTo(*pMat);
	}
	else
	{
		*pMat = Scalar(0,0,0);
	}

	return true;
}

bool _SortingImgServer::console(int &iY)
{
	IF_F(!this->_DetectorBase::console(iY));

	string msg;

	return true;
}

}
