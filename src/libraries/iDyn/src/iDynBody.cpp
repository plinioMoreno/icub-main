/* 
* Copyright (C) 2010 RobotCub Consortium, European Commission FP6 Project IST-004370
* Author: Serena Ivaldi, Matteo Fumagalli
* email:   serena.ivaldi@iit.it, matteo.fumagalli@iit.it
* website: www.robotcub.org
* Permission is granted to copy, distribute, and/or modify this program
* under the terms of the GNU General Public License, version 2 or any
* later version published by the Free Software Foundation.
*
* A copy of the license can be found at
* http://www.robotcub.org/icub/license/gpl.txt
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
* Public License for more details
*/

#include <iCub/iDyn/iDyn.h>
#include <iCub/iDyn/iDynBody.h>
#include <stdio.h>
#include <iostream>
#include <iomanip>

using namespace std;
using namespace yarp;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::math;
using namespace iCub::ctrl;
using namespace iCub::iKin;
using namespace iCub::iDyn;

//====================================
//
//		RIGID BODY TRANSFORMATION
//
//====================================

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
RigidBodyTransformation::RigidBodyTransformation(iDynLimb *_limb, const yarp::sig::Matrix &_H, const string &_info, bool _hasSensor, const FlowType kin, const FlowType wre, const NewEulMode _mode, unsigned int verb)
{
	limb = _limb;
	kinFlow = kin;
	wreFlow = wre;
	mode = _mode;
	info = _info;
	hasSensor=_hasSensor;
	F.resize(3);	F.zero();
	Mu.resize(3);	Mu.zero();
	w.resize(3);	w.zero();
	dw.resize(3);	dw.zero();
	ddp.resize(3);	ddp.zero();
	H.resize(4,4); H.eye();
	setRBT(_H);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
RigidBodyTransformation::~RigidBodyTransformation()
{
	// never delete the limb!! only stop pointing at it
	limb = NULL;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool RigidBodyTransformation::setRBT(const Matrix &_H)
{
	if((_H.cols()==4)&&(_H.rows()==4))		
	{
		H=_H;
		return true;
	}
	else
	{
		if(verbose) fprintf(stderr,"RigidBodyTransformation: could not set RBT due to wrong sized matrix H: %ld,%ld instead of (4,4). Setting identity as default. \n",_H.cols(),_H.rows());
		H.resize(4,4);
		H.eye();
		return false;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool RigidBodyTransformation::setKinematic(const Vector &wNode, const Vector &dwNode, const Vector &ddpNode)
{
	// set the RBT kinematic variables to the ones of the node
	w = wNode;
	dw = dwNode;
	ddp = ddpNode;
	//now apply the RBT transformation
	computeKinematic();
	// send the kinematic information to the limb - the limb knows whether it is on base or on end-eff
	return limb->initKinematicNewtonEuler(w,dw,ddp);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool RigidBodyTransformation::setKinematicMeasure(const Vector &w0, const Vector &dw0, const Vector &ddp0)
{
	return limb->initKinematicNewtonEuler(w0,dw0,ddp0);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool RigidBodyTransformation::setWrench(const Vector &FNode, const Vector &MuNode)
{
	//set the RBT force/moment with the one of the node
	F = FNode;
	Mu = MuNode;
	// now apply the RBT transformation
	computeWrench();
	// send the wrench to the limb - the limb knows whether it is on base or on end-eff 
	return limb->initWrenchNewtonEuler(F,Mu);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool RigidBodyTransformation::setWrenchMeasure(const Vector &F0, const Vector &Mu0)
{
	return limb->initWrenchNewtonEuler(F0,Mu0);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool RigidBodyTransformation::setWrenchMeasure(iDynSensor *sensor, const Vector &Fsens, const Vector &Musens)
{
	return sensor->setSensorMeasures(Fsens,Musens);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::getRBT() const			
{
	return H;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::getR()					
{
	return H.submatrix(0,2,0,2);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::getR6() const					
{
	Matrix R(6,6); R.zero();		
	for(int i=0;i<3;i++)
	{
		for(int j=0;j<3;j++)
		{
			R(i,j) = H(i,j);
			R(i+3,j+3) = R(i,j);
		}
	}
	return R;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector RigidBodyTransformation::getr(bool proj)	
{
	if(proj==false)
		return H.submatrix(0,2,0,3).getCol(3);
	else
		return (-1.0 * getR().transposed() * (H.submatrix(0,2,0,3)).getCol(3));
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void RigidBodyTransformation::getKinematic(Vector &wNode, Vector &dwNode, Vector &ddpNode)
{
	//read w,dw,ddp from the limb and stores them into the RBT variables
	limb->getKinematicNewtonEuler(w,dw,ddp);

	//now compute according to transformation
	computeKinematic();

	//apply the kinematics computations to the node
	wNode = w;
	dwNode = dw;
	ddpNode = ddp;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void RigidBodyTransformation::getWrench(Vector &FNode, Vector &MuNode)
{
	//read F,Mu from the limb and stores them into the RBT variables
	limb->getWrenchNewtonEuler(F,Mu);

	//now compute according to transformation
	computeWrench();

	//apply the wrench computations to the node
	FNode = FNode + F;
	MuNode = MuNode + Mu;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void RigidBodyTransformation::setInfoFlow(const FlowType kin, const FlowType wre)
{
	kinFlow=kin;
	wreFlow=wre;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
FlowType RigidBodyTransformation::getKinematicFlow() const
{
	return kinFlow;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
FlowType RigidBodyTransformation::getWrenchFlow() const
{
	return wreFlow;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void RigidBodyTransformation::computeKinematic()
{
	if(this->kinFlow== RBT_NODE_IN)
	{
		// note: these computations are similar to the Backward ones in 
		// OneLinkNewtonEuler: compute AngVel/AngAcc/LinAcc Backward
		// but here are fastened and adapted to the RBT
		// note: w,dw,ddp are already set with the ones coming from the LIMB

		switch(mode)
		{
		case DYNAMIC:
		case DYNAMIC_CORIOLIS_GRAVITY:
		case DYNAMIC_W_ROTOR:
			ddp = getR() * ( ddp - cross(dw,getr(true)) - cross(w,cross(w,getr(true))) ) ;
			w	= getR() * w ;	
			dw	= getR() * dw ;	
			break;
		case STATIC:	
			w	= 0.0;
			dw	= 0.0;
			ddp = getR() * ddp;
			break;
		}
	}
	else
	{
		// note: these computations are similar to the Forward ones in 
		// OneLinkNewtonEuler: compute AngVel/AngAcc/LinAcc 
		// but here are fastened and adapted to the RBT
		// note: w,dw,ddp are already set with the ones coming from the NODE

		switch(mode)
		{
		case DYNAMIC:
		case DYNAMIC_CORIOLIS_GRAVITY:
		case DYNAMIC_W_ROTOR:
			ddp = getR().transposed() * (ddp + cross(dw,getr(true)) + cross(w,cross(w,getr(true))) );
			w	= getR().transposed() * w ;			
			dw	= getR().transposed() * dw ;
			break;
		case STATIC:	
			w	= 0.0;
			dw	= 0.0;
			ddp =  getR().transposed() * ddp ;
			break;
		}
	
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void RigidBodyTransformation::computeWrench()
{
	if(this->wreFlow== RBT_NODE_IN)
	{
		// note: these computations are similar to the Backward ones in 
		// OneLinkNewtonEuler: compute Force/Moment Backward
		// but here are fastened and adapted to the RBT
		// note: no switch(mode) is necessary because all modes have the same formula
		// note: F,Mu are already set with the ones coming from the LIMB

		Mu = cross( getr(), getR()*F ) + getR() * Mu ;
		F  = getR() * F;
	}
	else
	{
		// note: these computations are similar to the Forward ones in 
		// OneLinkNewtonEuler: compute Force/Moment Forward 
		// but here are fastened and adapted to the RBT
		// note: no switch(mode) is necessary because all modes have the same formula
		// note: F,Mu are already set with the ones coming from the NODE

		Mu = getR().transposed() * ( Mu - cross(getr(),getR() * F ));
		F  = getR().transposed() * F ;
		
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool RigidBodyTransformation::isSensorized() const
{
	return hasSensor;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void RigidBodyTransformation::computeLimbKinematic()
{
	limb->computeKinematicNewtonEuler();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void RigidBodyTransformation::computeLimbWrench()
{
	limb->computeWrenchNewtonEuler();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
unsigned int RigidBodyTransformation::getNLinks() const
{
	return limb->getN();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
unsigned int RigidBodyTransformation::getDOF() const
{
	return limb->getDOF();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::getH(const unsigned int iLink, const bool allLink)            
{ 
	return limb->getH(iLink,allLink);          
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::getH()                                                          
{ 
	return limb->getH();                   
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector RigidBodyTransformation::getEndEffPose(const bool axisRep)
{
	return limb->EndEffPose(axisRep);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::computeGeoJacobian(const unsigned int iLink, const Matrix &Pn, bool rbtRoto)
{
	if(rbtRoto==false)
		return limb->computeGeoJacobian(iLink,Pn);
	else
		return getR6() * limb->computeGeoJacobian(iLink,Pn);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::computeGeoJacobian(const unsigned int iLink, const Matrix &Pn, const Matrix &H0, bool rbtRoto)
{
	if(rbtRoto==false)
		return limb->computeGeoJacobian(iLink,Pn,H0);
	else
		return getR6() * limb->computeGeoJacobian(iLink,Pn,H0);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::computeGeoJacobian(const Matrix &Pn, bool rbtRoto)
{
	if(rbtRoto==false)
		return limb->computeGeoJacobian(Pn);
	else
		return getR6() * limb->computeGeoJacobian(Pn);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::computeGeoJacobian(const Matrix &Pn, const Matrix &H0, bool rbtRoto)
{
	if(rbtRoto==false)
		return limb->computeGeoJacobian(Pn,H0);
	else
		return getR6() * limb->computeGeoJacobian(Pn,H0);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::computeGeoJacobian(bool rbtRoto)
{
	if(rbtRoto==false)
		return limb->GeoJacobian();
	else
		return getR6() * limb->GeoJacobian();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::computeGeoJacobian(const unsigned int iLink, bool rbtRoto)
{
	if(rbtRoto==false)
		return limb->GeoJacobian(iLink);
	else
		return getR6() * limb->GeoJacobian(iLink);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::getH0() const
{
	return limb->getH0();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool RigidBodyTransformation::setH0(const Matrix &_H0)
{
	return limb->setH0(_H0);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    //---------------
	// jacobians COM
	//---------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::computeCOMJacobian(const unsigned int iLink, bool rbtRoto)  
{ 
    if(rbtRoto==false)
        return limb->computeCOMJacobian(iLink);
    else
        return getR6() * limb->computeCOMJacobian(iLink);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::computeCOMJacobian(const unsigned int iLink, const Matrix &Pn, bool rbtRoto)
{ 
   if(rbtRoto==false)
        return limb->computeCOMJacobian(iLink, Pn);
    else
        return getR6() * limb->computeCOMJacobian(iLink, Pn);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::computeCOMJacobian(const unsigned int iLink, const Matrix &Pn, const Matrix &_H0, bool rbtRoto)
{ 
   if(rbtRoto==false)
        return limb->computeCOMJacobian(iLink, Pn, _H0);
    else
        return getR6() * limb->computeCOMJacobian(iLink, Pn, _H0);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix RigidBodyTransformation::getHCOM(unsigned int iLink)
{ 
   return limb->getHCOM(iLink);
}




//====================================
//
//		i DYN NODE
//
//====================================
	
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
iDynNode::iDynNode(const NewEulMode _mode)
{
	rbtList.clear();
	mode = _mode;
	verbose = VERBOSE;
	zero();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
iDynNode::iDynNode(const string &_info, const NewEulMode _mode, unsigned int verb)
{
	info=_info;
	rbtList.clear();
	mode = _mode;
	verbose = verb;
	zero();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void iDynNode::zero()
{
	w.resize(3); w.zero();
	dw.resize(3); dw.zero();
	ddp.resize(3); ddp.zero();
	F.resize(3); F.zero();
	Mu.resize(3); Mu.zero();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void iDynNode::addLimb(iDynLimb *limb, const Matrix &H, const FlowType kinFlow, const FlowType wreFlow, bool hasSensor)
{
	string infoRbt = limb->getType() + " to node";
	RigidBodyTransformation rbt(limb,H,infoRbt,hasSensor,kinFlow,wreFlow,mode,verbose);
	rbtList.push_back(rbt);
}    
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix iDynNode::getRBT(unsigned int iLimb) const
{
    if(iLimb<rbtList.size())
    {
        return rbtList[iLimb].getRBT();
    }
    else
    {
        if(verbose) fprintf(stderr,"iDynNode: error, could not getRBT() due to out of range index: %ld , while we have %ld limbs. \n", iLimb, rbtList.size() );
        return Matrix(0,0);
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynNode::solveKinematics()
{
	unsigned int inputNode=0;
	
	//first find the limb (one!) which must get the measured kinematics data
	// e.g. the head gets this information from the inertial sensor on the head
	for(unsigned int i=0; i<rbtList.size(); i++)
	{
		if(rbtList[i].getKinematicFlow()==RBT_NODE_IN)			
		{
			// measures are already set
			//then compute the kinematics pass in that limb 
			rbtList[i].computeLimbKinematic();
			// and retrieve the kinematics data in the base/end
			rbtList[i].getKinematic(w,dw,ddp);		
			//check
			inputNode++;
		}
	}

	//just check if the input node is only one (as it should be)
	if(inputNode==1)
	{
		//now forward the kinematic input from limbs whose kinematic flow is input type
		for(unsigned int i=0; i<rbtList.size(); i++)
		{
			if(rbtList[i].getKinematicFlow()==RBT_NODE_OUT)
			{
				//init the kinematics with the node information
				rbtList[i].setKinematic(w,dw,ddp);
				//solve kinematics in that limb/chain
				rbtList[i].computeLimbKinematic();
			}
		}
		return true;
	
	}
	else
	{
		if(verbose)
        {
            fprintf(stderr,"iDynNode error: there are %ld limbs with Kinematic Flow = Input. Only one limb must have Kinematic Input from outside measurements/computations. \n",inputNode);
            fprintf(stderr,"Please check the coherence of the limb configuration in the node <%s> \n",info.c_str());
        }
		return false;
	}

}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynNode::solveKinematics(const Vector &w0, const Vector &dw0, const Vector &ddp0)
{
	unsigned int inputNode=0;
	
	//first find the limb (one!) which must get the measured kinematics data
	// e.g. the head gets this information from the inertial sensor on the head
	for(unsigned int i=0; i<rbtList.size(); i++)
	{
		if(rbtList[i].getKinematicFlow()==RBT_NODE_IN)			
		{
			rbtList[i].setKinematicMeasure(w0,dw0,ddp0);
			//then compute the kinematics pass in that limb 
			rbtList[i].computeLimbKinematic();
			// and retrieve the kinematics data in the base/end
			rbtList[i].getKinematic(w,dw,ddp);		
			//check
			inputNode++;
		}
	}

	//just check if the input node is only one (as it should be)
	if(inputNode==1)
	{
		//now forward the kinematic input from limbs whose kinematic flow is input type
		for(unsigned int i=0; i<rbtList.size(); i++)
		{
			if(rbtList[i].getKinematicFlow()==RBT_NODE_OUT)
			{
				//init the kinematics with the node information
				rbtList[i].setKinematic(w,dw,ddp);
				//solve kinematics in that limb/chain
				rbtList[i].computeLimbKinematic();
			}
		}
		return true;
	
	}
	else
	{
		if(verbose)
        {
            fprintf(stderr,"iDynNode: error: there are %ld limbs with Kinematic Flow = Input. ",inputNode);
			fprintf(stderr," Only one limb must have Kinematic Input from outside measurements/computations. ");
            fprintf(stderr,"Please check the coherence of the limb configuration in the node <%s> \n", info.c_str());
        }
		return false;
	}

}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynNode::setKinematicMeasure(const Vector &w0, const Vector &dw0, const Vector &ddp0)
{
	if( (w0.length()==3)&&(dw0.length()==3)&&(ddp0.length()==3))
	{
		// find the limb (one!) which must get the measured kinematics data
		// e.g. the head gets this information from the inertial sensor on the head
		for(unsigned int i=0; i<rbtList.size(); i++)
		{
			if(rbtList[i].getKinematicFlow()==RBT_NODE_IN)			
            {
                rbtList[i].setKinematicMeasure(w0,dw0,ddp0);
                return true;
            }
		}
        if(verbose)
            fprintf(stderr,"iDynNode: error, there is not a node to set kinematic measure! \n");
		return false;
	}
	else
	{
		if(verbose)
        {
            fprintf(stderr,"iDynNode: error, could not set Kinematic measures due to wrong sized vectors: \n");
			fprintf(stderr," w,dw,ddp have lenght %ld,%ld,%ld instead of 3,3,3. \n",w0.length(),dw0.length(),ddp0.length());
        }
		return false;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynNode::solveWrench()
{
	unsigned int outputNode = 0;
	F.zero(); Mu.zero();

	//first get the forces/moments from each limb
	//assuming that each limb has been properly set with the outcoming measured
	//forces/moments which are necessary for the wrench computation
	for(unsigned int i=0; i<rbtList.size(); i++)
	{
		if(rbtList[i].getWrenchFlow()==RBT_NODE_IN)			
		{
			//compute the wrench pass in that limb
			rbtList[i].computeLimbWrench();
			//update the node force/moment with the wrench coming from the limb base/end
			// note that getWrench sum the result to F,Mu - because they are passed by reference
			// F = F + F[i], Mu = Mu + Mu[i]
			rbtList[i].getWrench(F,Mu);
			//check
			outputNode++;
		}
	}

	// node summation: already performed by each RBT
	// F = F + F[i], Mu = Mu + Mu[i]

	// at least one output node should exist 
	// however if for testing purposes only one limb is attached to the node, 
	// we can't avoid the computeWrench phase, but still we must remember that 
	// it is not correct, because the node must be in balance
	if(outputNode==rbtList.size())
	{
		if(verbose>1)
        {
            fprintf(stderr,"iDynNode: warning: there are no limbs with Wrench Flow = Output. ");
			fprintf(stderr," At least one limb must have Wrench Output for balancing forces in the node. ");
            fprintf(stderr,"Please check the coherence of the limb configuration in the node <%s> \n",info.c_str());
        }
	}

	//now forward the wrench output from the node to limbs whose wrench flow is output type
	for(unsigned int i=0; i<rbtList.size(); i++)
	{
		if(rbtList[i].getWrenchFlow()==RBT_NODE_OUT)
		{
			//init the wrench with the node information
			rbtList[i].setWrench(F,Mu);
			//solve wrench in that limb/chain
			rbtList[i].computeLimbWrench();
		}
	}
	return true;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynNode::solveWrench(const Matrix &FM)
{
	bool inputWasOk = setWrenchMeasure(FM);
	//now that all is set, we can really solveWrench()
	return ( solveWrench() && inputWasOk );
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynNode::solveWrench(const Matrix &Fm, const Matrix &Mm)
{
	bool inputWasOk = setWrenchMeasure(Fm,Mm);
	//now that all is set, we can really solveWrench()
	return ( solveWrench() && inputWasOk );
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynNode::setWrenchMeasure(const Matrix &FM)
{
	Vector fi(3); fi.zero();
	Vector mi(3); mi.zero();
	Vector FMi(6);FMi.zero();
	bool inputWasOk = true;

	//check how many limbs have wrench input
	int inputNode = howManyWrenchInputs(false);
		
	// input (eg measured) wrenches are stored in a 6xN matrix: each column is a 6x1 vector
	// with force/moment; N is the number of columns, ie the number of measured/input wrenches to the limb
	// the order is assumed coherent with the one built when adding limbs
	// eg: 
	// adding limbs: addLimb(limb1), addLimb(limb2), addLimb(limb3)
	// where limb1, limb3 have wrench flow input
	// passing wrenches: Matrix FM(6,2), FM.setcol(0,fm1), FM.setcol(1,fm3)
	if(FM.cols()<inputNode)
	{
		if(verbose)
        {
            fprintf(stderr,"iDynNode: could not solveWrench due to missing wrenches to initialize the computations: ");
			fprintf(stderr," only %ld f/m available instead of %ld. Using default values, all zero. \n ",FM.cols(),inputNode);
        }
		inputWasOk = false;
	}
	if(FM.rows()!=6)
	{
		if(verbose)
        {
            fprintf(stderr,"iDynNode: could not solveWrench due to wrong sized init wrenches: ");
			fprintf(stderr," %ld instead of 6 (3+3). Using default values, all zero. \n",FM.rows());
        }
		inputWasOk = false;
	}

	//set the measured/input forces/moments from each limb
	if(inputWasOk)
	{
		inputNode = 0;
		for(unsigned int i=0; i<rbtList.size(); i++)
		{
			if(rbtList[i].getWrenchFlow()==RBT_NODE_IN)			
			{
				// from the input matrix - read the input wrench
				FMi = FM.getCol(inputNode);
				fi[0]=FMi[0];fi[1]=FMi[1];fi[2]=FMi[2];
				mi[0]=FMi[3];mi[1]=FMi[4];mi[2]=FMi[5];
				inputNode++;
				//set the input wrench in the RBT->limb
				rbtList[i].setWrenchMeasure(fi,mi);
			}
		}
	}
	else
	{
		// default zero values if inputs are wrong sized
		for(unsigned int i=0; i<rbtList.size(); i++)
			if(rbtList[i].getWrenchFlow()==RBT_NODE_IN)			
				rbtList[i].setWrenchMeasure(fi,mi);
	}

	//now that all is set, we can really solveWrench()
	return inputWasOk;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynNode::setWrenchMeasure(const Matrix &Fm, const Matrix &Mm)
{
	bool inputWasOk = true;

	//check how many limbs have wrench input
	int inputNode = howManyWrenchInputs(false);
		
	// input (eg measured) wrenches are stored in two 3xN matrix: each column is a 3x1 vector
	// with force/moment; N is the number of columns, ie the number of measured/input wrenches to the limb
	// the order is assumed coherent with the one built when adding limbs
	// eg: 
	// adding limbs: addLimb(limb1), addLimb(limb2), addLimb(limb3)
	// where limb1, limb3 have wrench flow input
	// passing wrenches: Matrix Fm(3,2), Fm.setcol(0,f1), Fm.setcol(1,f3) and similar for moment
	if((Fm.cols()<inputNode)||(Mm.cols()<inputNode))
	{
		if(verbose)
        {
            fprintf(stderr,"iDynNode: could not setWrenchMeasure due to missing wrenches to initialize the computations: ");
			fprintf(stderr," only %ld/%ld f/m available instead of %ld/%ld. Using default values, all zero. \n",Fm.cols(),Mm.cols(),inputNode,inputNode);
        }
		inputWasOk = false;
	}
	if((Fm.rows()!=3)||(Mm.rows()!=3))
	{
		if(verbose)
        {
            fprintf(stderr,"iDynNode: could not setWrenchMeasure due to wrong sized init f/m: ");
			fprintf(stderr," %ld/%ld instead of 3/3. Using default values, all zero. \n",Fm.rows(),Mm.rows());
        }
		inputWasOk = false;
	}

	//set the measured/input forces/moments from each limb	
	if(inputWasOk)
	{
		inputNode = 0;
		for(unsigned int i=0; i<rbtList.size(); i++)
		{
			if(rbtList[i].getWrenchFlow()==RBT_NODE_IN)			
			{
				// from the input matrix
				//set the input wrench in the RBT->limb
				rbtList[i].setWrenchMeasure(Fm.getCol(inputNode),Mm.getCol(inputNode));
				inputNode++;
			}
		}
	}
	else
	{
		// default zero values if inputs are wrong sized
		Vector fi(3), mi(3); fi.zero(); mi.zero();
		for(unsigned int i=0; i<rbtList.size(); i++)	
			if(rbtList[i].getWrenchFlow()==RBT_NODE_IN)			
				rbtList[i].setWrenchMeasure(fi,mi);	
	}

	//now that all is set, we can really solveWrench()
	return inputWasOk ;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
unsigned int iDynNode::howManyWrenchInputs(bool afterAttach) const
{
	unsigned int inputNode = 0;

	//check how many limbs have wrench input
	for(unsigned int i=0; i<rbtList.size(); i++)
		if(rbtList[i].getWrenchFlow()==RBT_NODE_IN)			
			inputNode++;

	// if an attach has been done, we already set one wrench measure, so we don't have to do it again
	// and we expect FM (or F,M) to be smaller of 1
	if(afterAttach==true) inputNode--;

	return inputNode;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
unsigned int iDynNode::howManyKinematicInputs(bool afterAttach) const
{
	unsigned int inputNode = 0;

	//check how many limbs have kinematic input
	for(unsigned int i=0; i<rbtList.size(); i++)
        if(rbtList[i].getKinematicFlow()==RBT_NODE_IN)			
			inputNode++;

	// if an attach has been done, we already set one kinematic measure, so we don't have to do it again
	// actually we expect the number to be zero, because only one limb has kinematic measures
	if(afterAttach==true) inputNode--;

	return inputNode;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynNode::getForce() const {	return F;}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynNode::getMoment() const {return Mu;}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynNode::getAngVel() const {return w;}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynNode::getAngAcc() const {return dw;}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynNode::getLinAcc() const {return ddp;}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		//-----------------
		//    jacobians
		//-----------------


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix iDynNode::computeJacobian(unsigned int iChain)
{
    if(iChain<=rbtList.size())
    {
        return rbtList[iChain].computeGeoJacobian(false);
    }
    else
    {
		if(verbose) fprintf(stderr,"iDynNode: error, could not computeJacobian() due to out of range index: input limb has index %ld > %ld. Returning a null matrix. \n",iChain,rbtList.size());
		return Matrix(0,0);
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix iDynNode::computeJacobian(unsigned int iChain, unsigned int iLink)
{
    if(iChain<=rbtList.size())
    {
        // the check on iLink<=N is performed by iKinChain when calling GeoJacobian(iLink)
        // from the RBT
        return rbtList[iChain].computeGeoJacobian(iLink,false);
    }
    else
    {
		if(verbose) fprintf(stderr,"iDynNode: error, could not computeJacobian() due to out of range index: input limb has index %ld > %ld. Returning a null matrix. \n",iChain,rbtList.size());
		return Matrix(0,0);
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix iDynNode::computeJacobian(unsigned int iChainA, JacobType dirA, unsigned int iChainB, JacobType dirB)
{
	//first check param coherence:
	// - wrong limb index
	if( (iChainA > rbtList.size())||(iChainB > rbtList.size()) )
	{ 
		if(verbose) fprintf(stderr,"iDynNode: error, could not computeJacobian() due to out of range index: limbs have index %ld,%ld > %ld. Returning a null matrix. \n",iChainA,iChainB,rbtList.size());
		return Matrix(0,0);
	}
	// - jacobian .. in the same limb @_@
	if( iChainA==iChainB )
	{
		if(verbose) fprintf(stderr,"iDynNode: error, could not computeJacobian() due to weird index for chains %ld: same chains are selected. Please check the indexes or use the method iDynNode::computeJacobian(unsigned int iChain). Returning a null matrix. \n",iChainA);
		return Matrix(0,0);		
	}

	// params are ok, go on..

	// total number of joints = Ndof_A + Ndof_B
	// the total jacobian matrix
	Matrix J(6,rbtList[iChainA].getDOF() + rbtList[iChainB].getDOF()); J.zero();
	//the vector from the base-link (for the jac.) of limb A to the end-link (for the jac.) of limb B
	Matrix Pn; 
	// the two jacobians
	Matrix J_A; Matrix J_B;
	// from base-link (for the jac.) of limb A to base (for the jac.) of limb B
	Matrix H_A_Node;

	// compute the roto-transf matrix between the base of limb A and the base of limb B
	// this is used to set the correct reference of vector Pn
	// H_A_Node is used to initialize J_B properly
	compute_Pn_HAN(iChainA, dirA, iChainB, dirB, Pn, H_A_Node);

	// now compute jacobian of first and second limb, setting the correct Pn 
	// JA
	J_A = rbtList[iChainA].computeGeoJacobian(Pn,false);			
	// JB
	// note: set H_A_node as the H0 of the B chain, but does not modify the chain original H0 
	J_B = rbtList[iChainB].computeGeoJacobian(Pn,H_A_Node,false);

	// finally start building the Jacobian J=[J_A|J_B]
	unsigned int c=0;
	unsigned int JAcols = J_A.cols();
	unsigned int JBcols = J_B.cols();

	if(JAcols+JBcols!=J.cols())
	{
        fprintf(stderr,"iDynNode error: Jacobian should be 6x%ld instead is 6x%ld \n",J.cols(),(JAcols+JBcols));
        fprintf(stderr,"Note:  limb A:  N=%ld  DOF=%ld  \n",rbtList[iChainA].getNLinks(),rbtList[iChainA].getDOF());
        fprintf(stderr,"Note:  limb B:  N=%ld  DOF=%ld  \n",rbtList[iChainB].getNLinks(),rbtList[iChainB].getDOF());
		J.resize(6,JAcols+JBcols);
	}
	
	for(unsigned int r=0; r<6; r++)
	{
		for(c=0;c<JAcols;c++)
			J(r,c)=J_A(r,c);
		for(c=0;c<JBcols;c++)
			J(r,JAcols+c)=J_B(r,c);
	}
	
	// now return the jacobian
	return J;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix iDynNode::computeJacobian(unsigned int iChainA, JacobType dirA, unsigned int iChainB, unsigned int iLinkB, JacobType dirB)
{
	//first check param coherence:
	// - wrong limb index
	if( (iChainA > rbtList.size())||(iChainB > rbtList.size()) )
	{ 
		if(verbose) fprintf(stderr,"iDynNode: error, could not computeJacobian() due to out of range index: limbs have index %ld,%ld > %ld. Returning a null matrix.\n",iChainA,iChainB,rbtList.size());
		return Matrix(0,0);
	}
	// - jacobian .. in the same limb @_@
	if( iChainA==iChainB )
	{
        if(verbose) fprintf(stderr,"iDynNode: error, could not computeJacobian() due to weird index for chains %ld : same chains are selected. Please check the indexes or use the method iDynNode::computeJacobian(unsigned int iChain). Returning a null matrix.\n",iChainA);
		return Matrix(0,0);		
	}
    // - there's not a link with index iLink in that chain
    if( iLinkB >= rbtList[iChainB].getNLinks())
    {
   		if(verbose) fprintf(stderr,"iDynNode: error, could not computeJacobian() due to out of range index for chain %ld: the selected link %ld > %ld. Returning a null matrix. \n",iChainB,iLinkB,rbtList[iChainB].getNLinks());
		return Matrix(0,0);	 
    }

	// params are ok, go on..

	// total number of joints = Ndof_A + iLinkB
    // note that we are taking all the links in chainB, not only the DOF until iLinkB
	// the total jacobian matrix
	Matrix J(6,rbtList[iChainA].getDOF() + iLinkB ); J.zero();
	//the vector from the base-link (for the jac.) of limb A to the end-link (for the jac.) of limb B
	Matrix Pn; 
	// the two jacobians
	Matrix J_A; Matrix J_B;
	// from base-link (for the jac.) of limb A to base (for the jac.) of limb B
	Matrix H_A_Node;

	// compute the roto-transf matrix between the base of limb A and the base of limb B
	// this is used to set the correct reference of vector Pn
	// H_A_Node is used to initialize J_B properly
	compute_Pn_HAN(iChainA, dirA, iChainB, iLinkB, dirB, Pn, H_A_Node);

	// now compute jacobian of first and second limb, setting the correct Pn 
	// JA
	J_A = rbtList[iChainA].computeGeoJacobian(Pn,false);			
	// JB
	// note: set H_A_node as the H0 of the B chain, but does not modify the chain original H0 
	J_B = rbtList[iChainB].computeGeoJacobian(iLinkB,Pn,H_A_Node,false);

	// finally start building the Jacobian J=[J_A|J_B]
	unsigned int c=0;
	unsigned int JAcols = J_A.cols();
	unsigned int JBcols = J_B.cols();

	if(JAcols+JBcols!=J.cols())
	{
        fprintf(stderr,"iDynNode error: Jacobian should be 6x%ld instead is 6x%ld \n",J.cols(),(JAcols+JBcols));
        fprintf(stderr,"Note:  limb A:  N=%ld  DOF=%ld  \n",rbtList[iChainA].getNLinks(),rbtList[iChainA].getDOF());
        fprintf(stderr,"Note:  limb B:  N=%ld  DOF=%ld  iLinkB=%ld \n",rbtList[iChainB].getNLinks(),rbtList[iChainB].getDOF(),iLinkB);
		J.resize(6,JAcols+JBcols);
	}
	
	for(unsigned int r=0; r<6; r++)
	{
		for(c=0;c<JAcols;c++)
			J(r,c)=J_A(r,c);
		for(c=0;c<JBcols;c++)
			J(r,JAcols+c)=J_B(r,c);
	}
	
	// now return the jacobian
	return J;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void iDynNode::compute_Pn_HAN(unsigned int iChainA, JacobType dirA, unsigned int iChainB, JacobType dirB, Matrix &Pn, Matrix &H_A_Node)
{
	// compute the roto-transf matrix between the base of limb A and the base of limb B
	// this is used to set the correct reference of vector Pn
	if(dirA==JAC_KIN)
	{
		// H_A_Node = H_A * RBT_A^T * RBT_B 
		// note: RBT_A is transposed because we're going in the opposite direction wrt to one of the RBT
		H_A_Node = rbtList[iChainA].getH() * rbtList[iChainA].getRBT().transposed() * rbtList[iChainB].getRBT();
	}
	else //dirA==JAC_IKIN
	{
		// H_A_Node = H_A^-1 * RBT_A^T * RBT_B 
		H_A_Node = SE3inv(rbtList[iChainA].getH()) * rbtList[iChainA].getRBT().transposed() * rbtList[iChainB].getRBT();
	}

	if(dirB==JAC_KIN)
	{
		// Pn = H_A_Node * H_B 
		// Pn is the roto-transf matrix between base (of jac. in limb A) and end (of jac. in limb B)
		// it is needed because the two jacobians must refer to a common Pn
		Pn = H_A_Node * rbtList[iChainB].getH();
	}
	else //dirB==JAC_IKIN
	{
		// Pn = H_A_Node * H_B^-1
		Pn = H_A_Node * SE3inv(rbtList[iChainB].getH());
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void iDynNode::compute_Pn_HAN(unsigned int iChainA, JacobType dirA, unsigned int iChainB, unsigned int iLinkB, JacobType dirB, Matrix &Pn, Matrix &H_A_Node)
{
	// compute the roto-transf matrix between the base of limb A and the base of limb B
	// this is used to set the correct reference of vector Pn
	if(dirA==JAC_KIN)
	{
		// H_A_Node = H_A * RBT_A^T * RBT_B 
		// note: RBT_A is transposed because we're going in the opposite direction wrt to one of the RBT
		H_A_Node = rbtList[iChainA].getH() * rbtList[iChainA].getRBT().transposed() * rbtList[iChainB].getRBT();
	}
	else //dirA==JAC_IKIN
	{
		// H_A_Node = H_A^-1 * RBT_A^T * RBT_B 
		H_A_Node = SE3inv(rbtList[iChainA].getH()) * rbtList[iChainA].getRBT().transposed() * rbtList[iChainB].getRBT();
	}

	if(dirB==JAC_KIN)
	{
		// Pn = H_A_Node * H_B(iLinkB) 
		// Pn is the roto-transf matrix between base (of jac. in limb A) and end (of jac. in limb B)
		// it is needed because the two jacobians must refer to a common Pn
        // here for chain B we stop at the iLinkB link
        // allLink=true because we deal with all the links (even blocked ones): see motivation in the corresponding
        // jacobian with iLinkB
        Pn = H_A_Node * rbtList[iChainB].getH(iLinkB,true);
	}
	else //dirB==JAC_IKIN
	{
		// Pn = H_A_Node * H_B^-1
		Pn = H_A_Node * SE3inv(rbtList[iChainB].getH(iLinkB,true));
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynNode::computePose(unsigned int iChainA, JacobType dirA, unsigned int iChainB, JacobType dirB, const bool axisRep)
{
	//first check param coherence:
	// - wrong limb index
	if( (iChainA > rbtList.size())||(iChainB > rbtList.size()) )
	{ 
		if(verbose) fprintf(stderr,"iDynNode: error, could not computePose() due to out of range index: limbs have index %ld,%ld > %ld. Returning a null matrix. \n",iChainA,iChainB,rbtList.size());
		return Vector(0);
	}
	// - jacobian .. in the same limb @_@
	if( iChainA==iChainB )
	{
		if(verbose) fprintf(stderr,"iDynNode: error, could not computePose() due to weird index for chains %ld: same chains are selected. Please check the indexes or use the method iDynNode::computeJacobian(unsigned int iChain). Returning a null matrix. \n",iChainA);
		return Vector(0);		
	}

	// params are ok, go on..

	//the vector from the base-link (for the jac.) of limb A to the end-link (for the jac.) of limb B
	Matrix Pn, H_A_Node; 
	// the pose vector
	Vector v;

	// compute the roto-transf matrix between the base of limb A and the base of limb B
	// this is used to set the correct reference of vector Pn
	// just ignore H_A_Node
	compute_Pn_HAN(iChainA, dirA, iChainB, dirB, Pn, H_A_Node);

	// now compute the pose vector v (see iKin for more details)
	if (axisRep)
    {
        v.resize(7);
        Vector r=dcm2axis(Pn,verbose);
        v[0]=Pn(0,3);
        v[1]=Pn(1,3);
        v[2]=Pn(2,3);
        v[3]=r[0];
        v[4]=r[1];
        v[5]=r[2];
        v[6]=r[3];
    }
    else
    {
        v.resize(6);
		Vector r(3); r.zero();    
		// Euler Angles as XYZ (see dcm2angle.m)
		r[0]=atan2(-Pn(2,1),Pn(2,2));
		r[1]=asin(Pn(2,0));
		r[2]=atan2(-Pn(1,0),Pn(0,0));
        v[0]=Pn(0,3);
        v[1]=Pn(1,3);
        v[2]=Pn(2,3);
        v[3]=r[0];
        v[4]=r[1];
        v[5]=r[2];
    }
	
	return v;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynNode::computePose(unsigned int iChainA, JacobType dirA, unsigned int iChainB, unsigned int iLinkB, JacobType dirB, const bool axisRep)
{
	//first check param coherence:
	// - wrong limb index
	if( (iChainA > rbtList.size())||(iChainB > rbtList.size()) )
	{ 
		if(verbose) fprintf(stderr,"iDynNode: error, could not computePose() due to out of range index: limbs have index %ld,%ld > %ld. Returning a null matrix. \n",iChainA,iChainB,rbtList.size());
		return Vector(0);
	}
	// - jacobian .. in the same limb @_@
	if( iChainA==iChainB )
	{
		if(verbose) fprintf(stderr,"iDynNode: error, could not computePose() due to weird index for chains %ld: same chains are selected. Please check the indexes or use the method iDynNode::computeJacobian(unsigned int iChain). Returning a null matrix. \n",iChainA);
		return Vector(0);		
	}
    // - there's not a link with index iLink in that chain
    if( iLinkB >= rbtList[iChainB].getNLinks())
    {
   		if(verbose) fprintf(stderr,"iDynNode: error, could not computePose() due to out of range index for chain %ld: the selected link is %ld > %ld. Returning a null matrix. \n",iChainB,iLinkB,rbtList[iChainB].getNLinks());
		return Vector(0);	 
    }

	// params are ok, go on..

	//the vector from the base-link (for the jac.) of limb A to the end-link (for the jac.) of limb B
	Matrix Pn, H_A_Node; 
	// the pose vector
	Vector v;

	// compute the roto-transf matrix between the base of limb A and the base of limb B
	// this is used to set the correct reference of vector Pn
	// just ignore H_A_Node
	compute_Pn_HAN(iChainA, dirA, iChainB, iLinkB, dirB, Pn, H_A_Node);

	// now compute the pose vector v (see iKin for more details)
	if (axisRep)
    {
        v.resize(7);
        Vector r=dcm2axis(Pn,verbose);
        v[0]=Pn(0,3);
        v[1]=Pn(1,3);
        v[2]=Pn(2,3);
        v[3]=r[0];
        v[4]=r[1];
        v[5]=r[2];
        v[6]=r[3];
    }
    else
    {
        v.resize(6);
		Vector r(3); r.zero();    
		// Euler Angles as XYZ (see dcm2angle.m)
		r[0]=atan2(-Pn(2,1),Pn(2,2));
		r[1]=asin(Pn(2,0));
		r[2]=atan2(-Pn(1,0),Pn(0,0));
        v[0]=Pn(0,3);
        v[1]=Pn(1,3);
        v[2]=Pn(2,3);
        v[3]=r[0];
        v[4]=r[1];
        v[5]=r[2];
    }
	
	return v;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    //---------------
	// jacobians COM
	//---------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix iDynNode::computeCOMJacobian(unsigned int iChain, unsigned int iLink)
{
    if(iChain<=rbtList.size())
    {
        // the check on iLink<=N is performed by iDynChain when calling computeCOMJacobian(iLink)
        // from the RBT
        return rbtList[iChain].computeCOMJacobian(iLink,false);
    }
    else
    {
		if(verbose) fprintf(stderr,"iDynNode: error, could not computeCOMJacobian() due to out of range index: input limb has index %ld > %ld. Returning a null matrix. \n",iChain,rbtList.size());
		return Matrix(0,0);
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix iDynNode::computeCOMJacobian(unsigned int iChainA, JacobType dirA, unsigned int iChainB, unsigned int iLinkB, JacobType dirB)
{
	//first check param coherence:
	// - wrong limb index
	if( (iChainA > rbtList.size())||(iChainB > rbtList.size()) )
	{ 
		if(verbose) fprintf(stderr,"iDynNode: error, could not computeJacobian() due to out of range index: limbs have index %ld,%ld > %ld. Returning a null matrix. \n",iChainA,iChainB,rbtList.size());
		return Matrix(0,0);
	}
	// - jacobian .. in the same limb @_@
	if( iChainA==iChainB )
	{
		if(verbose) fprintf(stderr,"iDynNode: error, could not computeJacobian() due to weird index for chains %ld: same chains are selected. Please check the indexes or use the method iDynNode::computeJacobian(unsigned int iChain). Returning a null matrix. \n",iChainA);
		return Matrix(0,0);		
	}
    // - there's not a link with index iLink in that chain
    if( iLinkB >= rbtList[iChainB].getNLinks())
    {
        if(verbose) fprintf(stderr,"iDynNode: error, could not computeJacobian() due to out of range index for chain %ld: the selected link is %ld > %ld. Returning a null matrix. \n",iChainB,iLinkB,rbtList[iChainB].getNLinks());
		return Matrix(0,0);	 
    }

	// params are ok, go on..

	// total number of joints = Ndof_A + iLinkB
    // note that we are taking all the links in chainB, not only the DOF until iLinkB
	// the total jacobian matrix
	Matrix J(6,rbtList[iChainA].getDOF() + iLinkB ); J.zero();
	//the vector from the base-link (for the jac.) of limb A to the end-link (for the jac.) of limb B
	Matrix Pn; 
	// the two jacobians
	Matrix J_A; Matrix J_B;
	// from base-link (for the jac.) of limb A to base (for the jac.) of limb B
	Matrix H_A_Node;

	// compute the roto-transf matrix between the base of limb A and the base of limb B
	// H_A_Node is used to initialize J_B properly
    // Pn consider chain A, chain B until iLinkB, then the COM of iLinkB
	compute_Pn_HAN_COM(iChainA, dirA, iChainB, iLinkB, dirB, Pn, H_A_Node);

	// now compute jacobian of first and second limb, setting the correct Pn 
	// JA
	J_A = rbtList[iChainA].computeGeoJacobian(Pn,false);			
	// JB
	// note: set H_A_node as the H0 of the B chain, but does not modify the chain original H0 
	J_B = rbtList[iChainB].computeCOMJacobian(iLinkB,Pn,H_A_Node,false);

	// finally start building the Jacobian J=[J_A|J_B]
	unsigned int c=0;
	unsigned int JAcols = J_A.cols();
	unsigned int JBcols = J_B.cols();

	if(JAcols+JBcols!=J.cols())
	{
        fprintf(stderr,"iDynNode error: Jacobian should be 6x%ld instead is 6x%ld \n",J.cols(),(JAcols+JBcols));
        fprintf(stderr,"Note:  limb A:  N=%ld  DOF=%ld  \n",rbtList[iChainA].getNLinks(),rbtList[iChainA].getDOF());
        fprintf(stderr,"Note:  limb B:  N=%ld  DOF=%ld  iLinkB=%ld \n",rbtList[iChainB].getNLinks(),rbtList[iChainB].getDOF(),iLinkB);
		J.resize(6,JAcols+JBcols);
	}
	
	for(unsigned int r=0; r<6; r++)
	{
		for(c=0;c<JAcols;c++)
			J(r,c)=J_A(r,c);
		for(c=0;c<JBcols;c++)
			J(r,JAcols+c)=J_B(r,c);
	}
	
	// now return the jacobian
	return J;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void iDynNode::compute_Pn_HAN_COM(unsigned int iChainA, JacobType dirA, unsigned int iChainB, unsigned int iLinkB, JacobType dirB, Matrix &Pn, Matrix &H_A_Node)
{
	// compute the roto-transf matrix between the base of limb A and the base of limb B
	// this is used to set the correct reference of vector Pn
	if(dirA==JAC_KIN)
	{
		// H_A_Node = H_A * RBT_A^T * RBT_B 
		// note: RBT_A is transposed because we're going in the opposite direction wrt to one of the RBT
		H_A_Node = rbtList[iChainA].getH() * rbtList[iChainA].getRBT().transposed() * rbtList[iChainB].getRBT();
	}
	else //dirA==JAC_IKIN
	{
		// H_A_Node = H_A^-1 * RBT_A^T * RBT_B 
		H_A_Node = SE3inv(rbtList[iChainA].getH()) * rbtList[iChainA].getRBT().transposed() * rbtList[iChainB].getRBT();
	}

	if(dirB==JAC_KIN)
	{
		// Pn = H_A_Node * H_B(iLinkB) 
		// Pn is the roto-transf matrix between base (of jac. in limb A) and end (of jac. in limb B)
		// it is needed because the two jacobians must refer to a common Pn
        // here for chain B we stop at the iLinkB link
        // allLink=true because we deal with all the links (even blocked ones): see motivation in the corresponding
        // jacobian with iLinkB
        Pn = H_A_Node * rbtList[iChainB].getHCOM(iLinkB);
	}
	else //dirB==JAC_IKIN
	{
		// Pn = H_A_Node * H_B^-1
		Pn = H_A_Node * SE3inv(rbtList[iChainB].getHCOM(iLinkB));
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~






//====================================
//
//		i DYN SENSOR NODE
//
//====================================
	
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
iDynSensorNode::iDynSensorNode(const NewEulMode _mode)
:iDynNode(_mode)
{
	sensorList.clear();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
iDynSensorNode::iDynSensorNode(const string &_info, const NewEulMode _mode, unsigned int verb)
:iDynNode(_info,_mode,verb)
{
	sensorList.clear();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void iDynSensorNode::addLimb(iDynLimb *limb, const Matrix &H, const FlowType kinFlow, const FlowType wreFlow)
{
	iDynNode::addLimb(limb,H,kinFlow,wreFlow,false);
	iDynSensor *noSens = NULL;
	sensorList.push_back(noSens);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void iDynSensorNode::addLimb(iDynLimb *limb, const Matrix &H, iDynSensor *sensor, const FlowType kinFlow, const FlowType wreFlow)
{
	iDynNode::addLimb(limb,H,kinFlow,wreFlow,true);
	sensorList.push_back(sensor);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynSensorNode::solveWrench()
{
	unsigned int outputNode = 0;
    // set to zero the node force/moment
	F.zero(); Mu.zero();

	//first get the forces/moments from each limb
	//assuming that each limb has been properly set with the outcoming measured
	//forces/moments which are necessary for the wrench computation
	for(unsigned int i=0; i<rbtList.size(); i++)
	{
		if(rbtList[i].getWrenchFlow()==RBT_NODE_IN)			
		{
			//compute the wrench pass in that limb
			// if there's a sensor, we must use iDynSensor
			// otherwise we use the limb method as usual
			if(rbtList[i].isSensorized()==true)
				sensorList[i]->computeWrenchFromSensorNewtonEuler();
			else
				rbtList[i].computeLimbWrench();

			//update the node force/moment with the wrench coming from the limb base/end
			// note that getWrench sum the result to F,Mu - because they are passed by reference
			// F = F + F[i], Mu = Mu + Mu[i]
			rbtList[i].getWrench(F,Mu);
			//check
			outputNode++;
		}
	}

	// node summation: already performed by each RBT
	// F = F + F[i], Mu = Mu + Mu[i]

	// at least one output node should exist 
	// however if for testing purposes only one limb is attached to the node, 
	// we can't avoid the computeWrench phase, but still we must remember that 
	// it is not correct, because the node must be in balanc
	if(outputNode==rbtList.size())
	{
		if(verbose>1)
        {
            fprintf(stderr,"iDynNode: warning: there are no limbs with Wrench Flow = Output. At least one limb must have Wrench Output for balancing forces in the node. \n");
            fprintf(stderr,"Please check the coherence of the limb configuration in the node <%s>\n",info.c_str());
        }
	}

	//now forward the wrench output from the node to limbs whose wrench flow is output type
	// assuming they don't have a FT sensor
	for(unsigned int i=0; i<rbtList.size(); i++)
	{
		if(rbtList[i].getWrenchFlow()==RBT_NODE_OUT)
		{
			//init the wrench with the node information
			rbtList[i].setWrench(F,Mu);
			//solve wrench in that limb/chain
			rbtList[i].computeLimbWrench();
		}
	}
	return true;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynSensorNode::setWrenchMeasure(const Matrix &FM, bool afterAttach)
{
	Vector fi(3); fi.zero();
	Vector mi(3); mi.zero();
	Vector FMi(6);FMi.zero();
	bool inputWasOk = true;

	//check how many limbs have wrench input
	int inputNode = howManyWrenchInputs(afterAttach);
		
	// input (eg measured) wrenches are stored in a 6xN matrix: each column is a 6x1 vector
	// with force/moment; N is the number of columns, ie the number of measured/input wrenches to the limb
	// the order is assumed coherent with the one built when adding limbs
	// eg: 
	// adding limbs: addLimb(limb1), addLimb(limb2), addLimb(limb3)
	// where limb1, limb3 have wrench flow input
	// passing wrenches: Matrix FM(6,2), FM.setcol(0,fm1), FM.setcol(1,fm3)
	if(FM.cols()<inputNode)
	{
		if(verbose)
		{
			fprintf(stderr,"iDynNode: could not setWrenchMeasure due to missing wrenches to initialize the computations: only %ld f/m available instead of %ld. Using default values, all zero.\n",FM.cols(),inputNode);
			if(afterAttach==true)
			fprintf(stderr,"          Remember that the first limb receives wrench input during an attach from another node. \n");
		}
		inputWasOk = false;
	}
	if(FM.rows()!=6)
	{
		if(verbose)
			fprintf(stderr,"iDynNode: could not setWrenchMeasure due to wrong sized init wrenches: %ld instead of 6 (3+3). Using default values, all zero.\n",FM.rows());
		inputWasOk = false;
	}

	//set the input forces/moments from each limb at base/end
	// note: if afterAttach=true we set the wrench only in limbs 1,2,..
	// if afterAttach=false, we set the wrench in all limbs 0,1,2,..
	unsigned int startLimb=0;
	if(afterAttach) startLimb=1;

	//set the measured/input forces/moments from each limb
	if(inputWasOk)
	{
		inputNode = 0;
		for(unsigned int i=startLimb; i<rbtList.size(); i++)
		{
			if(rbtList[i].getWrenchFlow()==RBT_NODE_IN)			
			{
				// from the input matrix - read the input wrench
				FMi = FM.getCol(inputNode);
				fi[0]=FMi[0];fi[1]=FMi[1];fi[2]=FMi[2];
				mi[0]=FMi[3];mi[1]=FMi[4];mi[2]=FMi[5];
				inputNode++;
				//set the input wrench in the RBT->limb
				// if there's a sensor, set on the sensor
				// otherwise on base/end as usual
				if(rbtList[i].isSensorized()==true)
					rbtList[i].setWrenchMeasure(sensorList[i],fi,mi);
				else
					rbtList[i].setWrenchMeasure(fi,mi);
			}
		}
	}
	else
	{
		// default zero values if inputs are wrong sized
		for(unsigned int i=startLimb; i<rbtList.size(); i++)
			if(rbtList[i].getWrenchFlow()==RBT_NODE_IN)			
			{
				if(rbtList[i].isSensorized()==true)
					rbtList[i].setWrenchMeasure(sensorList[i],fi,mi);
				else
					rbtList[i].setWrenchMeasure(fi,mi);
			}
	}

	//now that all is set, we can really solveWrench()
	return inputWasOk ;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynSensorNode::setWrenchMeasure(const Matrix &Fm, const Matrix &Mm, bool afterAttach)
{
	bool inputWasOk = true;
	
	//check how many limbs have wrench input
	int inputNode = howManyWrenchInputs(afterAttach);
			
	// input (eg measured) wrenches are stored in two 3xN matrix: each column is a 3x1 vector
	// with force/moment; N is the number of columns, ie the number of measured/input wrenches to the limb
	// the order is assumed coherent with the one built when adding limbs
	// eg: 
	// adding limbs: addLimb(limb1), addLimb(limb2), addLimb(limb3)
	// where limb1, limb3 have wrench flow input
	// passing wrenches: Matrix Fm(3,2), Fm.setcol(0,f1), Fm.setcol(1,f3) and similar for moment
	if((Fm.cols()<inputNode)||(Mm.cols()<inputNode))
	{
		if(verbose)
		{
			fprintf(stderr,"iDynNode: could not setWrenchMeasure due to missing wrenches to initialize the computations: only %ld/%ld f/m available instead of %ld/%ld. Using default values, all zero.\n",Fm.cols(),Mm.cols(),inputNode,inputNode);
			if(afterAttach==true)
			fprintf(stderr,"          Remember that the first limb receives wrench input during an attach from another node.\n");
		}
		inputWasOk = false;
	}
	if((Fm.rows()!=3)||(Mm.rows()!=3))
	{
		if(verbose)	fprintf(stderr,"iDynNode: could not setWrenchMeasure due to wrong sized init f/m: %ld/%ld instead of 3/3. Using default values, all zero.\n",Fm.rows(),Mm.rows());
		inputWasOk = false;
	}

	//set the input forces/moments from each limb at base/end
	// note: if afterAttach=true we set the wrench only in limbs 1,2,..
	// if afterAttach=false, we set the wrench in all limbs 0,1,2,..
	unsigned int startLimb=0;
	if(afterAttach) startLimb=1;

	//set the measured/input forces/moments from each limb	
	if(inputWasOk)
	{
		inputNode = 0;
		for(unsigned int i=startLimb; i<rbtList.size(); i++)
		{
			if(rbtList[i].getWrenchFlow()==RBT_NODE_IN)			
			{
				// from the input matrix
				//set the input wrench in the RBT->limb
				// if there's a sensor, set on the sensor
				// otherwise on base/end as usual
				if(rbtList[i].isSensorized()==true)
					rbtList[i].setWrenchMeasure(sensorList[i],Fm.getCol(inputNode),Mm.getCol(inputNode));
				else
					rbtList[i].setWrenchMeasure(Fm.getCol(inputNode),Mm.getCol(inputNode));
				inputNode++;
			}
		}
	}
	else
	{
		// default zero values if inputs are wrong sized
		Vector fi(3), mi(3); fi.zero(); mi.zero();
		for(unsigned int i=startLimb; i<rbtList.size(); i++)	
			if(rbtList[i].getWrenchFlow()==RBT_NODE_IN)			
				rbtList[i].setWrenchMeasure(fi,mi);	
	}

	//now that all is set, we can call solveWrench() or update()...
	return inputWasOk ;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix iDynSensorNode::estimateSensorsWrench(const Matrix &FM, bool afterAttach)
{
	unsigned int inputNode = 0;
	unsigned int outputNode = 0;	
	unsigned int numSensor = 0;
	bool inputWasOk = true;
	Vector fi(3); fi.zero();
	Vector mi(3); mi.zero();
	Vector FMi(6);FMi.zero();
	Matrix ret;
	
	//reset node wrench
	F.zero(); Mu.zero();

	// solve kinematics
	solveKinematics();

	//check how many limbs have wrench input
	inputNode = howManyWrenchInputs(afterAttach);

	//first check if the input is correct
	if(FM.rows()!=6)
	{
		if(verbose)	fprintf(stderr,"iDynSensorNode: could not setWrenchMeasure due to wrong sized init wrenches matrix: %ld rows instead of 6 (3+3). Using default values, all zero.\n",FM.rows());
		inputWasOk = false;
	}
	if(FM.cols()!=inputNode)
	{
		if(verbose)
			fprintf(stderr,"iDynSensorNode: could not setWrenchMeasure due to wrong sized init wrenches: %ld instead of %ld. Using default values, all zero. \n",FM.cols(),inputNode);
		if(afterAttach==true)
			fprintf(stderr,"                Remember that the first limb receives wrench input during an attach from another node.\n");
		inputWasOk = false;
	}

	//set the input forces/moments from each limb at base/end
	// note: if afterAttach=true we set the wrench only in limbs 1,2,..
	// if afterAttach=false, we set the wrench in all limbs 0,1,2,..
	unsigned int startLimb=0;
	if(afterAttach) startLimb=1;

	if(inputWasOk)
	{
		inputNode = 0;
		for(unsigned int i=startLimb; i<rbtList.size(); i++)
		{
			if(rbtList[i].getWrenchFlow()==RBT_NODE_IN)			
			{
				// from the input matrix - read the input wrench
				FMi = FM.getCol(inputNode);
				fi[0]=FMi[0];fi[1]=FMi[1];fi[2]=FMi[2];
				mi[0]=FMi[3];mi[1]=FMi[4];mi[2]=FMi[5];
				inputNode++;
				//set the input wrench in the RBT->limb
				// set on base/end as usual
				rbtList[i].setWrenchMeasure(fi,mi);
			}
		}
	}
	else
	{
		// default zero values if inputs are wrong sized
		for(unsigned int i=startLimb; i<rbtList.size(); i++)
			if(rbtList[i].getWrenchFlow()==RBT_NODE_IN)			
				rbtList[i].setWrenchMeasure(fi,mi);		
	}

	//first get the forces/moments from each limb
	//assuming that each limb has been properly set with the outcoming measured
	//forces/moments which are necessary for the wrench computation
	for(unsigned int i=0; i<rbtList.size(); i++)
	{
		if(rbtList[i].getWrenchFlow()==RBT_NODE_IN)			
		{
			//compute the wrench pass in that limb
			// if there's a sensor, it's the same because here we estimate its value
			rbtList[i].computeLimbWrench();

			//update the node force/moment with the wrench coming from the limb base/end
			// note that getWrench sum the result to F,Mu - because they are passed by reference
			// F = F + F[i], Mu = Mu + Mu[i]
			rbtList[i].getWrench(F,Mu);
			//check
			outputNode++;
		}
	}

	// node summation: already performed by each RBT
	// F = F + F[i], Mu = Mu + Mu[i]

	// at least one output node should exist 
	// however if for testing purposes only one limb is attached to the node, 
	// we can't avoid the computeWrench phase, but still we must remember that 
	// it is not correct, because the node must be in balanc
	if(outputNode==rbtList.size())
	{
		if(verbose>1)
			fprintf(stderr,"iDynSensorNode: warning: there are no limbs with Wrench Flow = Output. ");
			fprintf(stderr," At least one limb must have Wrench Output for balancing forces in the node. ");
            fprintf(stderr,"Please check the coherence of the limb configuration in the node <%s> \n",info.c_str());
	}

	//now forward the wrench output from the node to limbs whose wrench flow is output type
	// assuming they don't have a FT sensor
	for(unsigned int i=0; i<rbtList.size(); i++)
	{
		if(rbtList[i].getWrenchFlow()==RBT_NODE_OUT)
		{
			//init the wrench with the node information
			rbtList[i].setWrench(F,Mu);
			//solve wrench in that limb/chain
			rbtList[i].computeLimbWrench();
		}
	}

	// now look for sensors
	numSensor = 0;
	for(unsigned int i=0; i<rbtList.size(); i++)
	{
		//now we can estimate the sensor wrench if there's a sensor
		// if has sensor, compute its wrench
		if(rbtList[i].isSensorized()==true)				
		{
			sensorList[i]->computeSensorForceMoment();
			numSensor++;
		}
	}

	//now build the matrix with wrenches
	if(numSensor>0)
	{
		ret.resize(6,numSensor);
		numSensor=0;
		for(unsigned int i=0; i<rbtList.size(); i++)
			if(rbtList[i].isSensorized()==true)		
			{
				ret.setCol(numSensor,sensorList[i]->getSensorForceMoment());
				numSensor++;
			}
	}
	return ret;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
unsigned int iDynSensorNode::howManySensors() const
{
	unsigned int numSensor = 0;
	for(unsigned int i=0; i<rbtList.size(); i++)
	{
		if(rbtList[i].isSensorized()==true)				
			numSensor++;
	}
	return numSensor;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



//====================================
//
//	     iDYN SENSOR TORSO NODE   
//
//====================================


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
iDynSensorTorsoNode::iDynSensorTorsoNode(const NewEulMode _mode, unsigned int verb)
:iDynSensorNode("torso_node",_mode,verb)
{}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
iDynSensorTorsoNode::iDynSensorTorsoNode(const string &_info,const NewEulMode _mode, unsigned int verb)
:iDynSensorNode(_info,_mode,verb)
{}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void iDynSensorTorsoNode::build()
{
	left	= new iCubArmNoTorsoDyn("left",KINFWD_WREBWD);
	right	= new iCubArmNoTorsoDyn("right",KINFWD_WREBWD);
	up		= new iCubNeckInertialDyn(KINBWD_WREBWD);

	leftSensor = new iDynSensorArmNoTorso(dynamic_cast<iCubArmNoTorsoDyn*>(left),mode,verbose);
	rightSensor= new iDynSensorArmNoTorso(dynamic_cast<iCubArmNoTorsoDyn*>(right),mode,verbose);

	HUp.resize(4,4);	HUp.zero();
	HLeft.resize(4,4);	HLeft.zero();
	HRight.resize(4,4);	HRight.zero();

	// order: head - right - left
	addLimb(up,HUp,RBT_NODE_IN,RBT_NODE_IN);
	addLimb(right,HRight,rightSensor,RBT_NODE_OUT,RBT_NODE_IN);
	addLimb(left,HLeft,leftSensor,RBT_NODE_OUT,RBT_NODE_IN);

	left_name = "default_up";
	right_name= "default_right";
	up_name	  = "default_left";
	name	  = "default_upper_torso";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
iDynSensorTorsoNode::~iDynSensorTorsoNode()
{
	delete rightSensor; rightSensor = NULL;
	delete leftSensor;	leftSensor = NULL;
	delete up;			up = NULL;
	delete right;		right = NULL;
	delete left;		left = NULL;

	rbtList.clear();
	sensorList.clear();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynSensorTorsoNode::setInertialMeasure(const Vector &w0, const Vector &dw0, const Vector &ddp0)
{
	return setKinematicMeasure(w0,dw0,ddp0);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynSensorTorsoNode::setSensorMeasurement(const Vector &FM_right, const Vector &FM_left)
{
	Matrix FM(6,2); FM.zero();
	if((FM_right.length()==6)&&(FM_left.length()==6))
	{
		FM.setCol(0,FM_right);
		FM.setCol(1,FM_left);
		return setWrenchMeasure(FM,true);
	}
	else
	{
		if(verbose) fprintf(stderr,"Node <%s> could not set sensor measurements properly due to wrong sized vectors. FM right/left have lenght %ld,%ld instead of 6,6. Setting everything to zero. \n",name.c_str(),FM_right.length(),FM_left.length());
		setWrenchMeasure(FM,true);
		return false;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynSensorTorsoNode::setSensorMeasurement(const Vector &FM_right, const Vector &FM_left, const Vector &FM_up)
{
	Matrix FM(6,3); FM.zero();
	if((FM_right.length()==6)&&(FM_left.length()==6)&&(FM_up.length()==6))
	{
		// order: up 0 - right 1 - left 2
		FM.setCol(0,FM_up);
		FM.setCol(1,FM_right);
		FM.setCol(2,FM_left);
		return setWrenchMeasure(FM,false);
	}
	else
	{
		if(verbose) fprintf(stderr,"Node <%s> could not set sensor measurements properly due to wrong sized vectors. FM up/right/left have lenght %ld,%ld,%ld instead of 6,6. Setting everything to zero. \n",name.c_str(),FM_up.length(),FM_right.length(),FM_left.length());
		setWrenchMeasure(FM);
		return false;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynSensorTorsoNode::update()
{
	bool isOk = true;
	isOk = solveKinematics();
	isOk = isOk && solveWrench();
	return isOk;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynSensorTorsoNode::update(const Vector &w0, const Vector &dw0, const Vector &ddp0, const Vector &FM_right, const Vector &FM_left, const Vector &FM_up)
{
	bool inputOk = true;

	if((FM_right.length()==6)&&(FM_left.length()==6)&&(FM_up.length()==6)&&(w0.length()==3)&&(dw0.length()==3)&&(ddp0.length()==3))
	{
		setInertialMeasure(w0,dw0,ddp0);
		setSensorMeasurement(FM_right,FM_left,FM_up);
		return update();
	}
	else
	{
		if(verbose)
        {
            fprintf(stderr,"Node <%s> error, could not update() due to wrong sized vectors. ",name.c_str());
			fprintf(stderr," w0,dw0,ddp0 have size %ld,%ld,%ld instead of 3,3,3. \n",w0.length(),dw0.length(),ddp0.length());
			fprintf(stderr," FM up/right/left have size %ld,%ld,%ld instead of 6,6,6. \n",FM_up.length(),FM_right.length(),FM_left.length());
			fprintf(stderr," Updating without new values.\n");
        }
		update();
		return false;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iDynSensorTorsoNode::update(const Vector &FM_right, const Vector &FM_left, bool afterAttach)
{
	if(afterAttach==false)
	{
		if(verbose)
        {
            fprintf(stderr,"Node <%s> error, could not update() due to missing wrench vectors. ",name.c_str());
			fprintf(stderr,"This type of update() only works after an attachTorso() or after having set the central limb wrench and kinematics variables. ");
			fprintf(stderr,"You should try with the other update() methods. ");
			fprintf(stderr,"Could not perform update(): exiting. \n");
        }
		return false;
	}

	if((FM_right.length()==6)&&(FM_left.length()==6))
	{
		Matrix FM(6,2); 
		FM.setCol(0,FM_right);
		FM.setCol(1,FM_left);
		setWrenchMeasure(FM,true);
		return update();
	}
	else
	{
		if(verbose)
        {
            fprintf(stderr,"Node <%s> error, could not update() due to wrong sized vectors. ",name.c_str());
			fprintf(stderr,"FM right/left have size %ld,%ld instead of 6,6. Updating without new values. \n",FM_right.length(),FM_left.length());
        }
		update();
		return false;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	//----------------
	//      GET
	//----------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix iDynSensorTorsoNode::getForces(const string &limbType)
{
	if(limbType==up_name)				return up->getForces();
	else if(limbType==left_name)		return left->getForces();
	else if(limbType==right_name)		return right->getForces();
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return Matrix(0,0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix iDynSensorTorsoNode::getMoments(const string &limbType)
{
	if(limbType==up_name)			return up->getMoments();
	else if(limbType==left_name)	return left->getMoments();
	else if(limbType==right_name)	return right->getMoments();
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return Matrix(0,0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynSensorTorsoNode::getTorques(const string &limbType)
{
	if(limbType==up_name)			return up->getTorques();
	else if(limbType==left_name)	return left->getTorques();
	else if(limbType==right_name)	return right->getTorques();
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return Vector(0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynSensorTorsoNode::getTorsoForce() const {	return F;}	
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynSensorTorsoNode::getTorsoMoment() const{	return Mu;}	
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynSensorTorsoNode::getTorsoAngVel() const{	return w;}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynSensorTorsoNode::getTorsoAngAcc() const{	return dw;}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynSensorTorsoNode::getTorsoLinAcc() const{	return ddp;}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	//------------------
	//    LIMB CALLS
	//------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynSensorTorsoNode::setAng(const string &limbType, const Vector &_q)
{
	if(limbType==up_name)			return up->setAng(_q);
	else if(limbType==left_name)	return left->setAng(_q);
	else if(limbType==right_name)	return right->setAng(_q);
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return Vector(0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynSensorTorsoNode::getAng(const string &limbType)
{
	if(limbType==up_name)			return up->getAng();
	else if(limbType==left_name)	return left->getAng();
	else if(limbType==right_name)	return right->getAng();
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return Vector(0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double iDynSensorTorsoNode::setAng(const string &limbType, const unsigned int i, double _q)
{
	if(limbType==up_name)			return up->setAng(i,_q);
	else if(limbType==left_name)	return left->setAng(i,_q);
	else if(limbType==right_name)	return right->setAng(i,_q);
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return 0.0;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double iDynSensorTorsoNode::getAng(const string &limbType, const unsigned int i)
{
	if(limbType==up_name)			return up->getAng(i);
	else if(limbType==left_name)	return left->getAng(i);
	else if(limbType==right_name)	return right->getAng(i);
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return 0.0;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynSensorTorsoNode::setDAng(const string &limbType, const Vector &_dq)
{
	if(limbType==up_name)			return up->setDAng(_dq);
	else if(limbType==left_name)	return left->setDAng(_dq);
	else if(limbType==right_name)	return right->setDAng(_dq);
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return Vector(0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynSensorTorsoNode::getDAng(const string &limbType)
{
	if(limbType==up_name)			return up->getDAng();
	else if(limbType==left_name)	return left->getDAng();
	else if(limbType==right_name)	return right->getDAng();
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return Vector(0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double iDynSensorTorsoNode::setDAng(const string &limbType, const unsigned int i, double _dq)
{
	if(limbType==up_name)			return up->setDAng(i,_dq);
	else if(limbType==left_name)	return left->setDAng(i,_dq);
	else if(limbType==right_name)	return right->setDAng(i,_dq);
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return 0.0;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double iDynSensorTorsoNode::getDAng(const string &limbType, const unsigned int i)    
{
	if(limbType==up_name)			return up->getDAng(i);
	else if(limbType==left_name)	return left->getDAng(i);
	else if(limbType==right_name)	return right->getDAng(i);
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return 0.0;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynSensorTorsoNode::setD2Ang(const string &limbType, const Vector &_ddq)
{
	if(limbType==up_name)			return up->setD2Ang(_ddq);
	else if(limbType==left_name)	return left->setD2Ang(_ddq);
	else if(limbType==right_name)	return right->setD2Ang(_ddq);
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return Vector(0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iDynSensorTorsoNode::getD2Ang(const string &limbType)
{
	if(limbType==up_name)			return up->getD2Ang();
	else if(limbType==left_name)	return left->getD2Ang();
	else if(limbType==right_name)	return right->getD2Ang();
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return Vector(0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double iDynSensorTorsoNode::setD2Ang(const string &limbType, const unsigned int i, double _ddq)
{
	if(limbType==up_name)			return up->setD2Ang(i,_ddq);
	else if(limbType==left_name)	return left->setD2Ang(i,_ddq);
	else if(limbType==right_name)	return right->setD2Ang(i,_ddq);
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return 0.0;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double iDynSensorTorsoNode::getD2Ang(const string &limbType, const unsigned int i)
{
	if(limbType==up_name)			return up->getD2Ang(i);
	else if(limbType==left_name)	return left->getD2Ang(i);
	else if(limbType==right_name)	return right->getD2Ang(i);
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return 0.0;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
unsigned int iDynSensorTorsoNode::getNLinks(const string &limbType) const
{
	if(limbType==up_name)			return up->getN();
	else if(limbType==left_name)	return left->getN();
	else if(limbType==right_name)	return right->getN();
	else
	{		
		if(verbose)	fprintf(stderr,"Node <%s> there's not a limb named %s. Only %s,%s,%s are available. \n",name.c_str(),limbType.c_str(),left_name.c_str(),right_name.c_str(),up_name.c_str());
		return 0;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



//====================================
//
//	        UPPER TORSO   
//
//====================================


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
iCubUpperTorso::iCubUpperTorso(const NewEulMode _mode, unsigned int verb)
:iDynSensorTorsoNode("upper-torso",_mode,verb)
{
	build();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void iCubUpperTorso::build()
{
	left	= new iCubArmNoTorsoDyn("left",KINFWD_WREBWD);
	right	= new iCubArmNoTorsoDyn("right",KINFWD_WREBWD);
	up		= new iCubNeckInertialDyn(KINBWD_WREBWD);

	leftSensor = new iDynSensorArmNoTorso(dynamic_cast<iCubArmNoTorsoDyn*>(left),mode,verbose);
	rightSensor= new iDynSensorArmNoTorso(dynamic_cast<iCubArmNoTorsoDyn*>(right),mode,verbose);

	HUp.resize(4,4);	HUp.eye();
	HLeft.resize(4,4);	HLeft.zero();
	HRight.resize(4,4);	HRight.zero();

	double theta = CTRL_DEG2RAD * (180.0-15.0);
	HLeft(0,0) = cos(theta);	HLeft(0,1) = 0.0;		HLeft(0,2) = sin(theta);	HLeft(0,3) = 0.003066;
	HLeft(1,0) = 0.0;			HLeft(1,1) = 1.0;		HLeft(1,2) = 0.0;			HLeft(1,3) = -0.049999;
	HLeft(2,0) = -sin(theta);	HLeft(2,1) = 0.0;		HLeft(2,2) = cos(theta);	HLeft(2,3) = -0.110261;
	HLeft(3,3) = 1.0;	
	HRight(0,0) = -cos(theta);	HRight(0,1) = 0.0;  HRight(0,2) = -sin(theta);	HRight(0,3) = 0.00294;
	HRight(1,0) = 0.0;			HRight(1,1) = -1.0; HRight(1,2) = 0.0;			HRight(1,3) = -0.050;
	HRight(2,0) = -sin(theta);	HRight(2,1) = 0.0;	HRight(2,2) = cos(theta);	HRight(2,3) = 0.10997;
	HRight(3,3) = 1.0;

	// order: head - right arm - left arm
	addLimb(up,HUp,RBT_NODE_IN,RBT_NODE_IN);
	addLimb(right,HRight,rightSensor,RBT_NODE_OUT,RBT_NODE_IN);
	addLimb(left,HLeft,leftSensor,RBT_NODE_OUT,RBT_NODE_IN);

	left_name = "left_arm";
	right_name= "right_arm";
	up_name	  = "head";
	name	  = "upper_torso";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



//====================================
//
//	        LOWER TORSO   
//
//====================================


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
iCubLowerTorso::iCubLowerTorso(const NewEulMode _mode, unsigned int verb)
:iDynSensorTorsoNode("lower-torso",_mode,verb)
{
	build();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void iCubLowerTorso::build()
{
	left	= new iCubLegNoTorsoDyn("left",KINFWD_WREBWD);
	right	= new iCubLegNoTorsoDyn("right",KINFWD_WREBWD);
	up		= new iCubTorsoDyn("lower",KINBWD_WREBWD);

	leftSensor = new iDynSensorLeg(dynamic_cast<iCubLegDyn*>(left),mode,verbose);
	rightSensor= new iDynSensorLeg(dynamic_cast<iCubLegDyn*>(right),mode,verbose);

	HUp.resize(4,4);	HUp.zero(); HUp=eye(4,4);
	HLeft.resize(4,4);	HRight.resize(4,4);
	
	// left and right RBT have a common part
	HLeft.zero();
	HLeft(2,0)=-1.0;	HLeft(0,1)=-1.0;	
	HLeft(2,2)=-1.0;	HLeft(3,3)=1.0;		
	HRight = HLeft;
	// left part
	HLeft(0,3)=-0.1199;	HLeft(2,3)=0.0681;
	// right part
	HRight(0,3)=-0.1199;HRight(2,3)=-0.0681;

	// order: torso - right leg - left leg
	addLimb(up,HUp,RBT_NODE_IN,RBT_NODE_IN);
	addLimb(right,HRight,rightSensor,RBT_NODE_OUT,RBT_NODE_IN);
	addLimb(left,HLeft,leftSensor,RBT_NODE_OUT,RBT_NODE_IN);

	left_name = "left_leg";
	right_name= "right_leg";
	up_name	  = "torso";
	name	  = "lower_torso";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


//====================================
//
//	        iCUB WHOLE BODY  
//
//====================================

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
iCubWholeBody::iCubWholeBody(const NewEulMode mode, unsigned int verbose)
{
	//create all limbs
	upperTorso = new iCubUpperTorso(mode,verbose);
	lowerTorso = new iCubLowerTorso(mode,verbose);
	
	//now create a connection between upperTorso node and Torso ( lowerTorso->up == Torso )
	Matrix H(4,4); H.eye();
	rbt = new RigidBodyTransformation(lowerTorso->up,H,"connection between lower and upper torso",false,RBT_NODE_OUT,RBT_NODE_OUT,mode,verbose);

}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
iCubWholeBody::~iCubWholeBody()
{
	delete upperTorso; upperTorso = NULL;
	delete lowerTorso; lowerTorso = NULL;
	delete rbt;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void iCubWholeBody::attachTorso()
{
	// take the w,dw,ddp,F,Mu from upperTorso, apply the RBT, and set the 
	// kinematic and wrench variables into the lowerTorso
	//rbt->setKinematic(upperTorso->getTorsoAngVel(),upperTorso->getTorsoAngAcc(),upperTorso->getTorsoLinAcc());

	lowerTorso->setKinematicMeasure(upperTorso->getTorsoAngVel(),upperTorso->getTorsoAngAcc(),upperTorso->getTorsoLinAcc());

	//lowerTorso->setKinematicMeasure(upperTorso->getTorsoAngVel(),upperTorso->getTorsoAngAcc(),upperTorso->getTorsoLinAcc());
	// revision6047 lowerTorso->setInertialMeasure(upperTorso->getTorsoAngVel(),upperTorso->getTorsoAngAcc(),upperTorso->getTorsoLinAcc());

	//rbt->setWrench(upperTorso->getTorsoForce(),upperTorso->getTorsoMoment());	

	lowerTorso->up->initWrenchNewtonEuler(upperTorso->getTorsoForce(),upperTorso->getTorsoMoment());

	//lowerTorso->setWrenchMeasure(upperTorso->getTorsoForce(),upperTorso->getTorsoMoment());
	// revision 6047 lowerTorso->up->initWrenchNewtonEuler(upperTorso->getTorsoForce(),upperTorso->getTorsoMoment());

}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iCubWholeBody::updateAll(const Vector &w0, const Vector &dw0, const Vector &ddp0, const Vector &FM_head, 
							  const Vector &FM_right_arm, const Vector &FM_left_arm,
							  const Vector &FM_right_leg, const Vector &FM_left_leg)
{
	bool isOk = true;
	isOk = isOk && upperTorso->update(w0,dw0,ddp0,FM_right_arm,FM_left_arm,FM_head);
	attachTorso();
	isOk = isOk && lowerTorso->update(FM_right_leg,FM_left_leg,NODE_AFTER_ATTACH);
	return isOk;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~





//====================================
//
//	     iCUB UPPER BODY   
//
//====================================


//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
iCubUpperBody::iCubUpperBody(const NewEulMode _mode, unsigned int verb)
:iDynSensorNode("icub_upper_body",_mode,verb)
{
    build();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void iCubUpperBody::build()
{
	leftArm	    = new iCubArmNoTorsoDyn("left",KINFWD_WREBWD);
	rightArm    = new iCubArmNoTorsoDyn("right",KINFWD_WREBWD);
	head		= new iCubNeckInertialDyn(KINBWD_WREBWD);
    torso       = new iCubTorsoDyn("lower",KINBWD_WREBWD);
	
	leftSensor = new iDynSensorArmNoTorso(leftArm,mode,verbose);
	rightSensor= new iDynSensorArmNoTorso(rightArm,mode,verbose);

    /// HHead: roto-translational matrix defining the head base frame with respect to the torso node
    /// HLeft: roto-translational matrix defining the left arm base frame with respect to the torso node
    /// HRight: roto-translational matrix defining the right arm base frame with respect to the torso node
    /// HTorso: roto-translational matrix defining the torso base frame with respect to the torso node

    Matrix HHead(4,4);	    HHead.eye();
	Matrix HLeft(4,4);	    HLeft.zero();
	Matrix HRight(4,4);	    HRight.zero();
    Matrix HTorso(4,4);     HTorso.eye();
    Matrix H0Torso(4,4);    H0Torso.zero();

	double theta = CTRL_DEG2RAD * (180.0-15.0);
	HLeft(0,0) = cos(theta);	HLeft(0,1) = 0.0;		HLeft(0,2) = sin(theta);	HLeft(0,3) = 0.003066;
	HLeft(1,0) = 0.0;			HLeft(1,1) = 1.0;		HLeft(1,2) = 0.0;			HLeft(1,3) = -0.049999;
	HLeft(2,0) = -sin(theta);	HLeft(2,1) = 0.0;		HLeft(2,2) = cos(theta);	HLeft(2,3) = -0.110261;
	HLeft(3,3) = 1.0;	
	HRight(0,0) = -cos(theta);	HRight(0,1) = 0.0;      HRight(0,2) = -sin(theta);	HRight(0,3) = 0.00294;
	HRight(1,0) = 0.0;			HRight(1,1) = -1.0;     HRight(1,2) = 0.0;			HRight(1,3) = -0.050;
	HRight(2,0) = -sin(theta);	HRight(2,1) = 0.0;	    HRight(2,2) = cos(theta);	HRight(2,3) = 0.10997;
	HRight(3,3) = 1.0;
    H0Torso(0,1)=-1.0;          H0Torso(1,2)=-1.0;      H0Torso(2,0)=1.0;           H0Torso(3,3)=1.0;

    // by default, we set the H0 matrix of the torso limb to be coherent with
    // the H0 in iCubArm (iKin) and iCubArmDyn (the iDyn version of iCubArm)
    // the main reason is to armonize with iKin the computation of the jacobians
	torso->setH0(H0Torso);

	// order: head - right arm - left arm - torso
	addLimb(head,       HHead,                  RBT_NODE_IN,    RBT_NODE_IN);
	addLimb(rightArm,   HRight, rightSensor,    RBT_NODE_OUT,   RBT_NODE_IN);
	addLimb(leftArm,    HLeft,  leftSensor,     RBT_NODE_OUT,   RBT_NODE_IN);
    addLimb(torso,      HTorso,                 RBT_NODE_OUT,   RBT_NODE_OUT);

}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
iCubUpperBody::~iCubUpperBody()
{
	delete rightSensor; rightSensor = NULL;
	delete leftSensor;	leftSensor = NULL;
	delete head;		head = NULL;
	delete rightArm;	rightArm = NULL;
	delete leftArm;		leftArm = NULL;
    delete torso;       torso = NULL;

	rbtList.clear();
	sensorList.clear();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iCubUpperBody::setInertialMeasure(const Vector &w0, const Vector &dw0, const Vector &ddp0)
{
	return setKinematicMeasure(w0,dw0,ddp0);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iCubUpperBody::setSensorsWrenchMeasure(const Vector &FM_right, const Vector &FM_left, const Vector &FM_head)
{
	Matrix FM(6,3); FM.zero();
    if((FM_right.length()==6)&&(FM_left.length()==6)&&FM_head.length()==6)
	{
        // order: head 0 - rightArm 1 - leftArm 2
        // note that there's not the torso, because the torso wrench
        // is set after the wrench phase in the node..
        FM.setCol(0,FM_head);
		FM.setCol(1,FM_right);
		FM.setCol(2,FM_left);
		return setWrenchMeasure(FM,false);
	}
	else
	{
		if(verbose)
			fprintf(stderr,"iCubUpperBody could not set sensor measurements properly due to wrong sized vectors. FM head/right/left have lenght %ld,%ld,%ld instead of 6,6. Setting everything to zero. \n",FM_head.length(),FM_right.length(),FM_left.length());
		return false;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iCubUpperBody::setH0(const string &limbType, const Matrix &H0)
{
    //setH0 already check the size of the input H0 and returns false if not 4x4
    if(limbType=="head")			return head->setH0(H0);
    else if(limbType=="left_arm")	return leftArm->setH0(H0);
    else if(limbType=="right_arm")	return rightArm->setH0(H0);
    else if(limbType=="torso")      return torso->setH0(H0);
    else
    {		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. ",limbType.c_str());
	    return false;
    }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iCubUpperBody::update()
{
	bool isOk = true;
	isOk = solveKinematics();
	isOk = isOk && solveWrench();
	return isOk;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iCubUpperBody::update(const Vector &w0, const Vector &dw0, const Vector &ddp0, const Vector &FM_right, const Vector &FM_left, const Vector &FM_head)
{
	bool inputOk = true;

	if((FM_right.length()==6)&&(FM_left.length()==6)&&(FM_head.length()==6)&&(w0.length()==3)&&(dw0.length()==3)&&(ddp0.length()==3))
	{
		setInertialMeasure(w0,dw0,ddp0);
		setSensorsWrenchMeasure(FM_right,FM_left,FM_head);
		return update();
	}
	else
	{
		if(verbose)
        { 
            fprintf(stderr,"iCubUpperBody: error, could not update() due to wrong sized vectors. ");
			fprintf(stderr," w0,dw0,ddp0 have size %ld,%ld,%ld instead of 3,3,3. ",w0.length(),dw0.length(),ddp0.length());
			fprintf(stderr," FM head,right/left arm have size %ld,%ld,%ld instead of 6,6,6. ",FM_head.length(),FM_right.length(),FM_left.length());
			fprintf(stderr," Updating without new values.\n");
        }
		update();
		return false;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
bool iCubUpperBody::update(const Vector &w0, const Vector &dw0, const Vector &ddp0, const Vector &FM_right, const Vector &FM_left)
{
	bool inputOk = true;

	if((FM_right.length()==6)&&(FM_left.length()==6)&&(w0.length()==3)&&(dw0.length()==3)&&(ddp0.length()==3))
	{
        Vector FM_head(6); FM_head.zero();
		setInertialMeasure(w0,dw0,ddp0);
		setSensorsWrenchMeasure(FM_right,FM_left,FM_head);
		return update();
	}
	else
	{
		if(verbose)
        {
            fprintf(stderr,"iCubUpperBody: error, could not update() due to wrong sized vectors. ");
			fprintf(stderr," w0,dw0,ddp0 have size %ld,%ld,%ld instead of 3,3,3. ",w0.length(),dw0.length(),ddp0.length());
			fprintf(stderr," FM right/left arm have size %ld,%ld instead of 6,6. ",FM_right.length(),FM_left.length());
			fprintf(stderr," Updating without new values.\n");
        }
		update();
		return false;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	//----------------
	//      GET
	//----------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix iCubUpperBody::getForces(const string &limbType)
{
	if(limbType=="head")				return head->getForces();
	else if(limbType=="left_arm")		return leftArm->getForces();
	else if(limbType=="right_arm")		return rightArm->getForces();
    else if(limbType=="torso")          return torso->getForces();
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return Matrix(0,0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix iCubUpperBody::getMoments(const string &limbType)
{
	if(limbType=="head")			return head->getMoments();
	else if(limbType=="left_arm")	return leftArm->getMoments();
	else if(limbType=="right_arm")	return rightArm->getMoments();
	else if(limbType=="torso")      return torso->getMoments();
    else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return Matrix(0,0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iCubUpperBody::getTorques(const string &limbType)
{
	if(limbType=="head")			return head->getTorques();
	else if(limbType=="left_arm")	return leftArm->getTorques();
	else if(limbType=="right_arm")	return rightArm->getTorques();
	else if(limbType=="torso")      return torso->getTorques();
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return Vector(0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iCubUpperBody::getNodeForce() const  {return F;}	
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iCubUpperBody::getNodeMoment() const {return Mu;}	
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iCubUpperBody::getNodeAngVel() const {return w;}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iCubUpperBody::getNodeAngAcc() const {return dw;}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iCubUpperBody::getNodeLinAcc() const {return ddp;}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	//------------------
	//    LIMB CALLS
	//------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iCubUpperBody::setAng(const string &limbType, const Vector &_q)
{
	if(limbType=="head")			return head->setAng(_q);
	else if(limbType=="left_arm")	return leftArm->setAng(_q);
	else if(limbType=="right_arm")	return rightArm->setAng(_q);
	else if(limbType=="torso")      return torso->setAng(_q);
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return Vector(0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iCubUpperBody::getAng(const string &limbType)
{
	if(limbType=="head")			return head->getAng();
	else if(limbType=="left_arm")	return leftArm->getAng();
	else if(limbType=="right_arm")	return rightArm->getAng();
	else if(limbType=="torso")      return torso->getAng();
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return Vector(0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double iCubUpperBody::setAng(const string &limbType, const unsigned int i, double _q)
{
	if(limbType=="head")			return head->setAng(i,_q);
	else if(limbType=="left_arm")	return leftArm->setAng(i,_q);
	else if(limbType=="right_arm")	return rightArm->setAng(i,_q);
	else if(limbType=="torso")      return torso->setAng(i,_q);
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return 0.0;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double iCubUpperBody::getAng(const string &limbType, const unsigned int i)
{
	if(limbType=="head")			return head->getAng(i);
	else if(limbType=="left_arm")	return leftArm->getAng(i);
	else if(limbType=="right_arm")	return rightArm->getAng(i);
	else if(limbType=="torso")      return torso->getAng(i);
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return 0.0;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iCubUpperBody::setDAng(const string &limbType, const Vector &_dq)
{
	if(limbType=="head")			return head->setDAng(_dq);
	else if(limbType=="left_arm")	return leftArm->setDAng(_dq);
	else if(limbType=="right_arm")	return rightArm->setDAng(_dq);
	else if(limbType=="torso")      return torso->setDAng(_dq);
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return Vector(0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iCubUpperBody::getDAng(const string &limbType)
{
	if(limbType=="head")			return head->getDAng();
	else if(limbType=="left_arm")	return leftArm->getDAng();
	else if(limbType=="right_arm")	return rightArm->getDAng();
	else if(limbType=="torso")      return torso->getDAng();
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return Vector(0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double iCubUpperBody::setDAng(const string &limbType, const unsigned int i, double _dq)
{
	if(limbType=="head")			return head->setDAng(i,_dq);
	else if(limbType=="left_arm")	return leftArm->setDAng(i,_dq);
	else if(limbType=="right_arm")	return rightArm->setDAng(i,_dq);
	else if(limbType=="torso")      return torso->setDAng(i,_dq);
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return 0.0;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double iCubUpperBody::getDAng(const string &limbType, const unsigned int i)    
{
	if(limbType=="head")			return head->getDAng(i);
	else if(limbType=="left_arm")	return leftArm->getDAng(i);
	else if(limbType=="right_arm")	return rightArm->getDAng(i);
	else if(limbType=="torso")      return torso->getDAng(i);
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return 0.0;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iCubUpperBody::setD2Ang(const string &limbType, const Vector &_ddq)
{
	if(limbType=="head")			return head->setD2Ang(_ddq);
	else if(limbType=="left_arm")	return leftArm->setD2Ang(_ddq);
	else if(limbType=="right_arm")	return rightArm->setD2Ang(_ddq);
	else if(limbType=="torso")      return torso->setD2Ang(_ddq);
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return Vector(0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iCubUpperBody::getD2Ang(const string &limbType)
{
	if(limbType=="head")			return head->getD2Ang();
	else if(limbType=="left_arm")	return leftArm->getD2Ang();
	else if(limbType=="right_arm")	return rightArm->getD2Ang();
	else if(limbType=="torso")      return torso->getD2Ang();
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return Vector(0);
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double iCubUpperBody::setD2Ang(const string &limbType, const unsigned int i, double _ddq)
{
	if(limbType=="head")			return head->setD2Ang(i,_ddq);
	else if(limbType=="left_arm")	return leftArm->setD2Ang(i,_ddq);
	else if(limbType=="right_arm")	return rightArm->setD2Ang(i,_ddq);
	else if(limbType=="torso")      return torso->setD2Ang(i,_ddq);
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return 0.0;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
double iCubUpperBody::getD2Ang(const string &limbType, const unsigned int i)
{
	if(limbType=="head")			return head->getD2Ang(i);
	else if(limbType=="left_arm")	return leftArm->getD2Ang(i);
	else if(limbType=="right_arm")	return rightArm->getD2Ang(i);
	else if(limbType=="torso")      return torso->getD2Ang(i);
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return 0.0;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
unsigned int iCubUpperBody::setJoints(const string &limbType, const Vector &_q, const Vector &_dq, const Vector &_ddq)
{
    Vector qchange = _q;
    if(limbType=="head")			{head->setD2Ang(_ddq); head->setDAng(_dq); qchange = head->setAng(_q);}
    else if(limbType=="left_arm")	{leftArm->setD2Ang(_ddq); leftArm->setDAng(_dq); qchange = leftArm->setAng(_q);}
    else if(limbType=="right_arm")	{rightArm->setD2Ang(_ddq); rightArm->setDAng(_dq); qchange = rightArm->setAng(_q);}
    else if(limbType=="torso")      {torso->setD2Ang(_ddq); torso->setDAng(_dq); qchange = torso->setAng(_q);}
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return 0;
	}
    if(qchange==_q)
        return 1;
    else
        return 2;
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
unsigned int iCubUpperBody::getNLinks(const string &limbType) const
{
	if(limbType=="head")			return head->getN();
	else if(limbType=="left_arm")	return leftArm->getN();
	else if(limbType=="right_arm")	return rightArm->getN();
	else if(limbType=="torso")      return torso->getN();
	else
	{		
        if(verbose)	fprintf(stderr,"iCubUpperBody: error there's not a limb named %s. Only 'head','right_arm','left_arm','torso' are available. \n",limbType.c_str());
		return 0;
	}
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix iCubUpperBody::estimateSensorsWrench(const Matrix &FM, bool afterAttach) 
{ 
    return iDynSensorNode::estimateSensorsWrench(FM,afterAttach); 
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


    //------------------
    //    JACOBIANS
    //------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Matrix iCubUpperBody::Jacobian_TorsoHead()			        {	return computeJacobian(3,JAC_KIN, 0,JAC_KIN);	}
Matrix iCubUpperBody::Jacobian_TorsoArmRight()		        {	return computeJacobian(3,JAC_KIN, 1,JAC_KIN);	}
Matrix iCubUpperBody::Jacobian_TorsoArmLeft()		        {	return computeJacobian(3,JAC_KIN, 2,JAC_KIN);	}
Matrix iCubUpperBody::Jacobian_HeadArmRight()		        {	return computeJacobian(0,JAC_IKIN,1,JAC_KIN);	}
Matrix iCubUpperBody::Jacobian_HeadArmLeft()		        {	return computeJacobian(0,JAC_IKIN,2,JAC_KIN);	}
Matrix iCubUpperBody::Jacobian_HeadTorso()			        {	return computeJacobian(0,JAC_IKIN,3,JAC_IKIN);	}
Matrix iCubUpperBody::Jacobian_ArmLeftArmRight()	        {	return computeJacobian(2,JAC_IKIN,1,JAC_KIN);	}
Matrix iCubUpperBody::Jacobian_ArmRightArmLeft()	        {	return computeJacobian(1,JAC_IKIN,2,JAC_KIN);	}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Vector iCubUpperBody::Pose_TorsoHead(bool axisRep)			{	return computePose(3,JAC_KIN, 0,JAC_KIN, axisRep);	}
Vector iCubUpperBody::Pose_TorsoArmRight(bool axisRep) 	    {	return computePose(3,JAC_KIN, 1,JAC_KIN, axisRep);	}		
Vector iCubUpperBody::Pose_TorsoArmLeft(bool axisRep)		{	return computePose(3,JAC_KIN, 2,JAC_KIN, axisRep);	}
Vector iCubUpperBody::Pose_HeadArmRight(bool axisRep)		{	return computePose(0,JAC_IKIN,1,JAC_KIN, axisRep);	}		
Vector iCubUpperBody::Pose_HeadArmLeft(bool axisRep)		{	return computePose(0,JAC_IKIN,2,JAC_KIN, axisRep);	}
Vector iCubUpperBody::Pose_HeadTorso(bool axisRep)			{	return computePose(0,JAC_IKIN,3,JAC_IKIN,axisRep);	}
Vector iCubUpperBody::Pose_ArmLeftArmRight(bool axisRep)	{	return computePose(2,JAC_IKIN,1,JAC_KIN, axisRep);	}
Vector iCubUpperBody::Pose_ArmRightArmLeft(bool axisRep)	{	return computePose(1,JAC_IKIN,2,JAC_KIN, axisRep);	}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~






