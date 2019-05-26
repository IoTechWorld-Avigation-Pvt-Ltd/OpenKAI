/*
 *  Created on: May 16, 2019
 *      Author: yankai
 */
#include "_OrientalMotor.h"

namespace kai
{

_OrientalMotor::_OrientalMotor()
{
	m_pMB = NULL;
	m_iSlave = 1;
	m_iData = 0;

	m_vStepRange.x = -1e5;
	m_vStepRange.y = 1e5;
	m_vSpeedRange.x = -4e6;
	m_vSpeedRange.y = 4e6;
	m_vAccelRange.x = 1;
	m_vAccelRange.y = 1e9;
	m_vBrakeRange.x = 1;
	m_vBrakeRange.y = 1e9;
	m_vCurrentRange.x = 0;
	m_vCurrentRange.y = 1000;

	m_cState.init();
	m_tState.init();
}

_OrientalMotor::~_OrientalMotor()
{
}

bool _OrientalMotor::init(void* pKiss)
{
	IF_F(!this->_ActuatorBase::init(pKiss));
	Kiss* pK = (Kiss*) pKiss;

	KISSm(pK, iSlave);
	KISSm(pK, iData);

	pK->v("stepFrom", &m_vStepRange.x);
	pK->v("stepTo", &m_vStepRange.y);
	pK->v("speedFrom", &m_vSpeedRange.x);
	pK->v("speedTo", &m_vSpeedRange.y);
	pK->v("accelFrom", &m_vAccelRange.x);
	pK->v("accelTo", &m_vAccelRange.y);
	pK->v("brakeFrom", &m_vBrakeRange.x);
	pK->v("brakeTo", &m_vBrakeRange.y);
	pK->v("currentFrom", &m_vCurrentRange.x);
	pK->v("currentTo", &m_vCurrentRange.y);

	pK->v("targetStep", &m_tState.m_step);
	pK->v("targetSpeed", &m_tState.m_speed);



	string iName;
	iName = "";
	F_ERROR_F(pK->v("_Modbus", &iName));
	m_pMB = (_Modbus*) (pK->root()->getChildInst(iName));
	IF_Fl(!m_pMB, iName + " not found");


	return true;
}

bool _OrientalMotor::start(void)
{
	m_bThreadON = true;
	int retCode = pthread_create(&m_threadID, 0, getUpdateThread, this);
	if (retCode != 0)
	{
		LOG_E(retCode);
		m_bThreadON = false;
		return false;
	}

	return true;
}

void _OrientalMotor::update(void)
{
	while (m_bThreadON)
	{
		this->autoFPSfrom();

		sendCMD();
		readStatus();

		this->autoFPSto();
	}
}

int _OrientalMotor::check(void)
{
	NULL__(m_pMB,-1);
	IF__(!m_pMB->bOpen(),-1);

	return 0;
}

void _OrientalMotor::sendCMD(void)
{
	IF_(check()<0);
//	IF_(m_tStampCmdSet <= m_tStampCmdSent);

	uint16_t pB[18];

	//88
	pB[0] = 0;
	pB[1] = m_iData;
	pB[2] = 0;
	pB[3] = 1;

	//92
	pB[4] = HIGH16(m_tState.m_step);
	pB[5] = LOW16(m_tState.m_step);
	pB[6] = HIGH16(m_tState.m_speed);
	pB[7] = LOW16(m_tState.m_speed);

	//96
	pB[8] = HIGH16(m_tState.m_accel);
	pB[9] = LOW16(m_tState.m_accel);
	pB[10] = HIGH16(m_tState.m_brake);
	pB[11] = LOW16(m_tState.m_brake);
	pB[12] = HIGH16(m_tState.m_current);
	pB[13] = LOW16(m_tState.m_current);
	pB[14] = 0;
	pB[15] = 1;
	pB[16] = 0;
	pB[17] = 0;

	int nR = 18;
	int r = m_pMB->writeRegisters(m_iSlave, 88, nR, pB);

	if(r == nR)
	{
		m_tStampCmdSent = m_tStampCmdSet;
	}
}

void _OrientalMotor::readStatus(void)
{
	IF_(check()<0);

	uint16_t pB[18];
	int nR = 6;
	int r = m_pMB->readRegisters(m_iSlave, 204, nR, pB);
	IF_(r != 6);

	m_cState.m_step = MAKE32(pB[0], pB[1]);
	LOG_I("step: "+i2str(m_cState.m_step));

//	m_cState.m_step = 0;
//	m_cState.m_step |= pB[0];
//	m_cState.m_step <<= 16;
//	m_cState.m_step |= pB[1];

//	LOG_I("speed: "+i2str(m_cState.m_speed));
}

bool _OrientalMotor::draw(void)
{
	IF_F(!this->_ActuatorBase::draw());
	Window* pWin = (Window*) this->m_pWindow;
	Mat* pMat = pWin->getFrame()->m();
	string msg;

	return true;
}

bool _OrientalMotor::console(int& iY)
{
	IF_F(!this->_ActuatorBase::console(iY));
	string msg;

	C_MSG("-- Current state --");
	C_MSG("step: " + i2str(m_cState.m_step));
	C_MSG("speed: " + i2str(m_cState.m_speed));

	C_MSG("-- Target state --");
	C_MSG("step: " + i2str(m_tState.m_step));
	C_MSG("speed: " + i2str(m_tState.m_speed));

	return true;
}

}
