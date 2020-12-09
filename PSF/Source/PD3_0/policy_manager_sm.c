/*******************************************************************************
  Policy Manager Source file

  Company:
    Microchip Technology Inc.

  File Name:
    policy_manager_sm.c

  Description:
    This file contains DPM State machine APIs
 *******************************************************************************/
/*******************************************************************************
Copyright � [2020] Microchip Technology Inc. and its subsidiaries.

Subject to your compliance with these terms, you may use Microchip software and
any derivatives exclusively with Microchip products. It is your responsibility
to comply with third party license terms applicable to your use of third party
software (including open source software) that may accompany Microchip software.

THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS".  NO WARRANTIES, WHETHER EXPRESS,
IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED WARRANTIES
OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE. IN
NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS BEEN
ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE.  TO THE FULLEST
EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN ANY WAY
RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY, THAT YOU
HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/

#include <psf_stdinc.h>

void DPM_Init(UINT8 u8PortNum)
{
    UINT8 u8CfgPowerRole = DPM_GET_CONFIGURED_POWER_ROLE(u8PortNum);

    /*Assigning u8CfgPowerRole in places of both power and data roles because,
     during init, power and data roles will be tied together and they have same values.
     PD_ROLE_SOURCE - PD_ROLE_DFP - Value is 1
     PD_ROLE_SINK - PD_ROLE_UFP - Value is 0
     PD_ROLE_DRP - PD_ROLE_TOGGLING - Value is 2*/
    
    DPM_UpdateDataRole(u8PortNum, u8CfgPowerRole);
    DPM_UpdatePowerRole(u8PortNum, u8CfgPowerRole); 
#if (TRUE == INCLUDE_PD_SINK)
    gasDPM[u8PortNum].u16SinkOperatingCurrInmA = DPM_0mA;        
#endif

    /*Update PD spec revision, power and data roles in u8DPMConfigData*/
	gasDPM[u8PortNum].u8DPMConfigData |= ((CONFIG_PD_DEFAULT_SPEC_REV  << DPM_DEFAULT_PD_SPEC_REV_POS)\
            | (u8CfgPowerRole << DPM_DEFAULT_POWER_ROLE_POS) \
            | (u8CfgPowerRole << DPM_DEFAULT_DATA_ROLE_POS));
   
    /*Update PD spec revision in u32DPMStatus*/
    gasDPM[u8PortNum].u32DPMStatus |= (CONFIG_PD_DEFAULT_SPEC_REV << DPM_CURR_PD_SPEC_REV_POS);
    gasDPM[u8PortNum].u16InternalEvntInProgress = SET_TO_ZERO;
    gasDPM[u8PortNum].u16DPMInternalEvents = SET_TO_ZERO;
    #if (TRUE == INCLUDE_POWER_FAULT_HANDLING)
	gasDPM[u8PortNum].u8VBUSPowerGoodTmrID = MAX_CONCURRENT_TIMERS;
	gasDPM[u8PortNum].u8PowerFaultISR = SET_TO_ZERO;
	gasDPM[u8PortNum].u8VBUSPowerFaultCount = RESET_TO_ZERO;
	gasDPM[u8PortNum].u8PowerFaultFlags = SET_TO_ZERO;
    /*VCONN OCS related variables*/
    gasDPM[u8PortNum].u8VCONNGoodtoSupply = TRUE;
    gasDPM[u8PortNum].u8VCONNPowerGoodTmrID = MAX_CONCURRENT_TIMERS;
    gasDPM[u8PortNum].u8VCONNPowerFaultCount = SET_TO_ZERO;
    #endif

    #if (TRUE == INCLUDE_PD_SOURCE_PPS)
    gasDPM[u8PortNum].u8AlertType = SET_TO_ZERO;
    gasDPM[u8PortNum].u8StatusEventFlags = SET_TO_ZERO;;
    gasDPM[u8PortNum].u8RealTimeFlags = SET_TO_ZERO;
    gasDPM[u8PortNum].u8StsClearTmrID = MAX_CONCURRENT_TIMERS;
    #endif

    #if (TRUE == INCLUDE_PD_VCONN_SWAP)
    gasDPM[u8PortNum].u8VCONNSwapWaitTmrID = MAX_CONCURRENT_TIMERS;
    #endif /*INCLUDE_PD_VCONN_SWAP*/

    #if (TRUE == INCLUDE_PD_PR_SWAP)
    gasDPM[u8PortNum].u8PRSwapWaitTmrID = MAX_CONCURRENT_TIMERS;
    #endif 
    
    #if (TRUE == INCLUDE_PD_DR_SWAP)
    gasDPM[u8PortNum].u8DRSwapWaitTmrID = MAX_CONCURRENT_TIMERS;
    #endif
    
    #if (TRUE == INCLUDE_PD_VDM)
    gasDPM[u8PortNum].u8VDMBusyTmrID = MAX_CONCURRENT_TIMERS;
    #endif 

    #if (TRUE == INCLUDE_PD_ALT_MODE)
    gasDPM[u8PortNum].u8AMETmrID = MAX_CONCURRENT_TIMERS;
    #endif     
}
/********************************************************************************************/

void DPM_StateMachineInit(void)
{
	for (UINT8 u8PortNum = SET_TO_ZERO; u8PortNum < CONFIG_PD_PORT_COUNT; u8PortNum++)
  	{        
        if (UPD_PORT_ENABLED == DPM_GET_CONFIGURED_PORT_EN(u8PortNum))
        {
		  	/* Init UPD350 GPIO */
		  	UPD_InitGPIO (u8PortNum);
			
            #if(TRUE == INCLUDE_UPD_HPD)
            /*Init UPD350 to support HPD*/
            UPD_HPDInit(u8PortNum);
            #endif

#if(TRUE == INCLUDE_PD_DRP)
            /*Type-C UPD350 register configuration for a port*/
            if(PD_ROLE_DRP == DPM_GET_DEFAULT_POWER_ROLE(u8PortNum))
            {
                TypeC_InitDRPPort(u8PortNum);
				
				/*For DRP, PRL_Init will be done once a valid DRP partner is matched*/
            }
            else
#endif
            {
                TypeC_InitPort(u8PortNum);
                
                /* Protocol Layer initialization for all the port present */
                PRL_Init (u8PortNum);
            }
        }
    }
}
/*******************************************************************************/

void DPM_RunStateMachine (UINT8 u8PortNum)
{
    MCHP_PSF_HOOK_DPM_PRE_PROCESS(u8PortNum);       

    /* Handle Client Requests if any */
    DPM_ClientRequestHandler (u8PortNum);
    
    /* Handle Internal Events if any */
    DPM_InternalEventHandler (u8PortNum);
    
	/* Handle Power Faults if any */
	#if (TRUE == INCLUDE_POWER_FAULT_HANDLING)
		DPM_PowerFaultHandler (u8PortNum);
	#endif
        
    /* Handle AltMode events if any */
    #if(TRUE == INCLUDE_PD_ALT_MODE)    
        DPM_AltModeEventHandler (u8PortNum);
    #endif
    
    /* Handle Power Throttling Bank Switch */
    #if (TRUE == INCLUDE_POWER_THROTTLING)
        PT_HandleBankSwitch (u8PortNum);
    #endif          
        
    /* Run Type C State machine*/
    TypeC_RunStateMachine (u8PortNum);
    
    /* Run Policy engine State machine*/
    PE_RunStateMachine (u8PortNum);  
    
    /* UPD Power Management */
    #if (TRUE == INCLUDE_POWER_MANAGEMENT_CTRL)
        UPD_PwrManagementCtrl (u8PortNum);
    #endif
}

/*********************************************************************************/
#if (TRUE == INCLUDE_POWER_FAULT_HANDLING)
static void DPM_ClearPowerFaultFlags(UINT8 u8PortNum)
{
    /*ISR flag is cleared by disabling the interrupt*/
    MCHP_PSF_HOOK_DISABLE_GLOBAL_INTERRUPT();
    gasDPM[u8PortNum].u8PowerFaultISR = SET_TO_ZERO;
    MCHP_PSF_HOOK_ENABLE_GLOBAL_INTERRUPT();
}
void DPM_PowerFaultHandler(UINT8 u8PortNum)
{
  	/* Incase detach reset the Power Fault handling variables*/
    if (((TYPEC_UNATTACHED_SRC == gasTypeCcontrol[u8PortNum].u8TypeCState) &&
		    (TYPEC_UNATTACHED_SRC_INIT_SS == gasTypeCcontrol[u8PortNum].u8TypeCSubState))||
				 ((TYPEC_UNATTACHED_SNK == gasTypeCcontrol[u8PortNum].u8TypeCState)))
    {
		/* Enable Fault PIO to detect OCS as it would have been disabled as part of
         Power fault handling*/        
		UPD_EnableInputPIO (u8PortNum, eUPDPIO_FAULT_IN);
        
		/* Kill the timer*/
        PDTimer_Kill (gasDPM[u8PortNum].u8VBUSPowerGoodTmrID);
		
		/*Setting the u8VBUSPowerGoodTmrID to MAX_CONCURRENT_TIMERS to indicate that
    	TimerID does not hold any valid timer IDs anymore*/
        gasDPM[u8PortNum].u8VBUSPowerGoodTmrID = MAX_CONCURRENT_TIMERS;
		
		/* Setting the power fault count to Zero */
        if(FALSE == (gasDPM[u8PortNum].u8PowerFaultFlags & DPM_TYPEC_ERR_RECOVERY_FLAG_MASK))
        {
            gasDPM[u8PortNum].u8VBUSPowerFaultCount = SET_TO_ZERO;
        }
        
        gasDPM[u8PortNum].u8PowerFaultFlags = SET_TO_ZERO;
        	
        /*******Resetting the VCONN OCS related variables************/
        
        /*Setting VCONNGoodtoSupply flag as true*/        
        gasDPM[u8PortNum].u8VCONNGoodtoSupply = TRUE;
        
        /* Killing the VCONN power good timer*/
        PDTimer_Kill (gasDPM[u8PortNum].u8VCONNPowerGoodTmrID);
        
        /*Setting the u8VCONNPowerGoodTmrID to MAX_CONCURRENT_TIMERS to indicate that
    	TimerID does not hold any valid timer IDs anymore*/
        gasDPM[u8PortNum].u8VCONNPowerGoodTmrID = MAX_CONCURRENT_TIMERS;
        
        /*Resetting the VCONN OCS fault count to Zero */
        gasDPM[u8PortNum].u8VCONNPowerFaultCount = SET_TO_ZERO;
        
        /*ISR flag for OVP,UVP,OCP,VCONN OCS is cleared by disabling the interrupt*/
        MCHP_PSF_HOOK_DISABLE_GLOBAL_INTERRUPT();
        gasDPM[u8PortNum].u8PowerFaultISR = SET_TO_ZERO;
        MCHP_PSF_HOOK_ENABLE_GLOBAL_INTERRUPT();
        
    } /*end of if detach check*/
	
    if(TRUE == (gasDPM[u8PortNum].u8PowerFaultFlags & DPM_HR_COMPLETE_WAIT_MASK))
    { 
        if((ePE_SRC_TRANSITION_TO_DEFAULT_POWER_ON_SS == gasPolicyEngine[u8PortNum].ePESubState) ||
				 (ePE_SNK_STARTUP == gasPolicyEngine[u8PortNum].ePEState))
        {
            /*Checks whether VCONN max power fault count exceeds*/
            if(gasDPM[u8PortNum].u8VCONNPowerFaultCount >= (gasCfgStatusData.sPerPortData[u8PortNum].u8VCONNMaxFaultCnt))
            {            
                /*Setting the VCONN Good to Supply Flag as False*/
                gasDPM[u8PortNum].u8VCONNGoodtoSupply = FALSE;
            }
            
            /*Checks whether VBUS max power fault count exceeds*/
            if (gasDPM[u8PortNum].u8VBUSPowerFaultCount >= (gasCfgStatusData.sPerPortData[u8PortNum].u8VBUSMaxFaultCnt))
            {
				/* Disable the receiver*/
                PRL_EnableRx (u8PortNum, FALSE);
				/* kill all the timers*/
                PDTimer_KillPortTimers (u8PortNum);		
				/* set the fault count to zero */
                gasDPM[u8PortNum].u8VBUSPowerFaultCount = SET_TO_ZERO;
				
                DEBUG_PRINT_PORT_STR (u8PortNum, "PWR_FAULT: HRCompleteWait Reseted\r\n");
				
                if (PD_ROLE_SOURCE == DPM_GET_CURRENT_POWER_ROLE(u8PortNum))
                {			
					/* Assign an idle state wait for detach*/
                    gasTypeCcontrol[u8PortNum].u8TypeCSubState = TYPEC_ATTACHED_SRC_IDLE_SS;
                }
                else
                { 
					/* Assign an idle state wait for detach*/
                    gasTypeCcontrol[u8PortNum].u8TypeCSubState = TYPEC_ATTACHED_SNK_IDLE_SS;
                }
				/* Assign an idle state wait for detach*/
                gasPolicyEngine[u8PortNum].ePEState = ePE_INVALIDSTATE;
                DEBUG_PRINT_PORT_STR (u8PortNum, "PWR_FAULT: Entered SRC/SNK Powered OFF state\r\n");
                
                (void)DPM_NotifyClient(u8PortNum, eMCHP_PSF_PORT_POWERED_OFF);
            }/*end of if condition of VBUS max count exceed check*/
            else
            {
                /* Enable Fault PIO to detect OCS as it would have been disabled as part of
                    Power fault handling*/
                UPD_EnableInputPIO (u8PortNum, eUPDPIO_FAULT_IN);
                
                #if (TRUE == INCLUDE_PD_SOURCE_PPS)
                /*On completion Hard Reset mechanism for VBUS fault initiate an alert message*/
                if (gasDPM[u8PortNum].u8AlertType & (DPM_ALERT_TYPE_OVP | DPM_ALERT_TYPE_OCP))
                {
                    DPM_RegisterInternalEvent(u8PortNum, DPM_INT_EVT_INITIATE_ALERT);
                }
                #endif
            } /*end of else condition of VBUS max count exceed check*/
			/* Reset Wait for HardReset Complete bit*/
            gasDPM[u8PortNum].u8PowerFaultFlags &= (~DPM_HR_COMPLETE_WAIT_MASK);
        }
    } /*end if DPM_HR_COMPLETE_WAIT_MASK check*/
    
    if (gasDPM[u8PortNum].u8PowerFaultISR)
    {
        DEBUG_PRINT_PORT_STR(u8PortNum, "DPM Fault Handling\r\n");
        /*If VCONN OCS is present , kill the VCONN power good timer*/
        if(gasDPM[u8PortNum].u8PowerFaultISR & DPM_POWER_FAULT_VCONN_OCS)
        {
            if(FALSE == DPM_NotifyClient(u8PortNum, eMCHP_PSF_VCONN_PWR_FAULT))
            {
                /*Clear the Power fault flag and return*/
                DPM_ClearPowerFaultFlags(u8PortNum);
                return;
            }
            /*Kill the VCONN Power fault timer*/
            PDTimer_Kill (gasDPM[u8PortNum].u8VCONNPowerGoodTmrID);
             /*Setting the u8VCONNPowerGoodTmrID to MAX_CONCURRENT_TIMERS to indicate that
            TimerID does not hold any valid timer IDs anymore*/
            gasDPM[u8PortNum].u8VCONNPowerGoodTmrID = MAX_CONCURRENT_TIMERS;
			
            DEBUG_PRINT_PORT_STR (u8PortNum, "PWR_FAULT: VCONN Power Fault\r\n");
        } /*end of if condition of VCONN OCS check*/
        if(gasDPM[u8PortNum].u8PowerFaultISR & ~DPM_POWER_FAULT_VCONN_OCS)
        { 
            #if (TRUE == INCLUDE_PD_SINK)
            /*Resetting EN_SINK IO status here as the EN_SINK is reset at 
               on detection of fault at ISR itself*/
            gasCfgStatusData.sPerPortData[u8PortNum].u32PortIOStatus &= \
                    ~(DPM_PORT_IO_EN_SINK_STATUS);
            #endif
            if(FALSE == DPM_NotifyClient(u8PortNum, eMCHP_PSF_VBUS_PWR_FAULT))
            {
                /*Clear the Power fault flag and return*/
                DPM_ClearPowerFaultFlags(u8PortNum);
                return;
            }

#if(TRUE == INCLUDE_PD_SOURCE)            
            if(PD_ROLE_SOURCE == DPM_GET_CURRENT_POWER_ROLE(u8PortNum))
            {
                /*Toggle DC_DC EN on VBUS fault to reset the DC-DC controller*/
                PWRCTRL_ConfigDCDCEn(u8PortNum, FALSE);    
            }
            else
            {
                /*Do nothing*/
            }
#endif
            
            #if (TRUE == INCLUDE_UPD_PIO_OVERRIDE_SUPPORT)
            /*Clear PIO override enable*/
            UPD_RegByteClearBit (u8PortNum, UPD_PIO_OVR_EN,  UPD_PIO_OVR_2);
            #endif
            
            /* Kill Power Good Timer */
            PDTimer_Kill (gasDPM[u8PortNum].u8VBUSPowerGoodTmrID);
        
            /*Setting the u8VBUSPowerGoodTmrID to MAX_CONCURRENT_TIMERS to indicate that
            TimerID does not hold any valid timer IDs anymore*/
            gasDPM[u8PortNum].u8VBUSPowerGoodTmrID = MAX_CONCURRENT_TIMERS;
			
            DEBUG_PRINT_PORT_STR (u8PortNum, "PWR_FAULT: VBUS Power Fault\r\n");
        } /*end of if condition of VBUS Fault check*/
        
        if(PE_IMPLICIT_CONTRACT == PE_GET_PD_CONTRACT(u8PortNum))
        {
            if(TRUE == DPM_NotifyClient(u8PortNum, eMCHP_PSF_TYPEC_ERROR_RECOVERY))
            {
                /* Set it to Type C Error Recovery */
                DPM_SetTypeCState(u8PortNum, TYPEC_ERROR_RECOVERY, TYPEC_ERROR_RECOVERY_ENTRY_SS);

                gasDPM[u8PortNum].u8PowerFaultFlags |= DPM_TYPEC_ERR_RECOVERY_FLAG_MASK;
            
                /*Increment the fault count*/
                gasDPM[u8PortNum].u8VBUSPowerFaultCount++;

                if (gasDPM[u8PortNum].u8VBUSPowerFaultCount >= \
                        gasCfgStatusData.sPerPortData[u8PortNum].u8VBUSMaxFaultCnt)
                {
                    /* kill all the timers*/
                    PDTimer_KillPortTimers (u8PortNum);

                    /* set the fault count to zero */
                    gasDPM[u8PortNum].u8VBUSPowerFaultCount = SET_TO_ZERO;

                    DEBUG_PRINT_PORT_STR (u8PortNum, "PWR_FAULT: HRCompleteWait Reseted\r\n");

                    if (PD_ROLE_SOURCE == DPM_GET_CURRENT_POWER_ROLE(u8PortNum))
                    {			
                        /* Assign an idle state wait for detach*/
                        DPM_SetTypeCState(u8PortNum, TYPEC_ATTACHED_SRC, TYPEC_ATTACHED_SRC_IDLE_SS);
                    }
                    else
                    { 
                        /* Assign an idle state wait for detach*/
                        DPM_SetTypeCState(u8PortNum, TYPEC_ATTACHED_SNK, TYPEC_ATTACHED_SNK_IDLE_SS);
                    }

                    DEBUG_PRINT_PORT_STR (u8PortNum, "PWR_FAULT: Entered SRC/SNK Powered OFF state\r\n");

                    gasDPM[u8PortNum].u8PowerFaultFlags &= (~DPM_TYPEC_ERR_RECOVERY_FLAG_MASK);

                    (void)DPM_NotifyClient(u8PortNum, eMCHP_PSF_PORT_POWERED_OFF);
                }
            }
            else
            {
                /*Do nothing. If User application returns FALSE for 
                eMCHP_PSF_TYPEC_ERROR_RECOVERY notification, it is expected that
                the user application will raise a Port disable client request*/
            }
        } /*end of if condition that checks implicit contract*/
        else
        {          
            if(gasDPM[u8PortNum].u8PowerFaultISR & DPM_POWER_FAULT_VCONN_OCS)
            {           
                /*Increment the VCONN fault count*/
                gasDPM[u8PortNum].u8VCONNPowerFaultCount++;
                
                /*CC comparator will off once VCONN OCS is detected for implicit contract it is 
                 enabled as part of Type C error recovery. For explicit contract it is enabled here*/
                /*Enabling the CC Sampling on CC1 and CC2 lines*/
                TypeC_ConfigCCComp (u8PortNum, TYPEC_CC_COMP_CTL_CC1_CC2);
            }
            if(gasDPM[u8PortNum].u8PowerFaultISR & ~DPM_POWER_FAULT_VCONN_OCS)
            {
                #if (TRUE == INCLUDE_PD_SOURCE_PPS)
                /*If VBUS fault occurs in a Explicit contract, register the alert
                   type and wait for HardReset completion to initiate the Alert message*/
                if (gasDPM[u8PortNum].u8PowerFaultISR & (DPM_POWER_FAULT_UV | DPM_POWER_FAULT_VBUS_OCS))
                {   
                    gasDPM[u8PortNum].u8AlertType |= DPM_ALERT_TYPE_OCP;
                    gasDPM[u8PortNum].u8StatusEventFlags |= DPM_EVENT_TYPE_OCP;
                }
                else if (gasDPM[u8PortNum].u8PowerFaultISR & DPM_POWER_FAULT_OVP)
                {
                    gasDPM[u8PortNum].u8AlertType |= DPM_ALERT_TYPE_OVP;
                    gasDPM[u8PortNum].u8StatusEventFlags |= DPM_EVENT_TYPE_OVP;
                }
                else
                {
                    /* Do Nothing */
                }
               #endif /*INCLUDE_PD_SOURCE_PPS*/
                /*Increment the fault count*/
                gasDPM[u8PortNum].u8VBUSPowerFaultCount++;            
            }
			/* Send Hard reset*/
            PE_SendHardReset(u8PortNum);
			
			/* Set Wait for HardReset Complete bit*/
            gasDPM[u8PortNum].u8PowerFaultFlags |= DPM_HR_COMPLETE_WAIT_MASK;
        } /* end of else part of implicit contract check*/

#if(TRUE == INCLUDE_PD_SOURCE) 
        if(PD_ROLE_SOURCE == DPM_GET_CURRENT_POWER_ROLE(u8PortNum))
        {
            /* Enable DC_DC_EN to drive power*/        
            PWRCTRL_ConfigDCDCEn(u8PortNum, TRUE); 
        }
        else
        {
            /*Do nothing*/
        }
#endif
        
        #if (TRUE == INCLUDE_UPD_PIO_OVERRIDE_SUPPORT)
        /*Enable PIO override enable*/
        UPD_RegByteSetBit (u8PortNum, UPD_PIO_OVR_EN,  UPD_PIO_OVR_2);
        #endif
            
		/*Clear the Power fault flag*/
        DPM_ClearPowerFaultFlags(u8PortNum);
    }
}
void DPM_VCONNPowerGood_TimerCB (UINT8 u8PortNum, UINT8 u8DummyVariable)
{
	/* Set the timer Id to Max Value*/
 	gasDPM[u8PortNum].u8VCONNPowerGoodTmrID = MAX_CONCURRENT_TIMERS;	
	/* Resetting the VCONN fault Count*/
	gasDPM[u8PortNum].u8VCONNPowerFaultCount = RESET_TO_ZERO;
    
    /*Notify that port is recovered form VCONN fault*/
    (void)DPM_NotifyClient(u8PortNum, eMCHP_PSF_RECOVERED_FRM_VCONN_PWR_FAULT);
}

void DPM_HandleExternalVBUSFault(UINT8 u8PortNum, UINT8 u8FaultType)
{
    /*Set power fault due to external source only  if power fault is not
     detected by internal mechanism */
    if (!gasDPM[u8PortNum].u8PowerFaultISR)
    {
        #if (TRUE == INCLUDE_PD_SOURCE)
        if(PD_ROLE_SINK != DPM_GET_CURRENT_POWER_ROLE(u8PortNum)) /*Port role is either Source or DRP*/
        {
            /*Disable VBUS_EN on detection of external fault*/
            UPD_GPIOUpdateOutput(u8PortNum, gasCfgStatusData.sPerPortData[u8PortNum].u8Pio_EN_VBUS, 
                    gasCfgStatusData.sPerPortData[u8PortNum].u8Mode_EN_VBUS, (UINT8)UPD_GPIO_DE_ASSERT);
        }
        #endif
        /*VBUS OCS flag is set for DPM to handle the VBUS fault*/
        MCHP_PSF_HOOK_DISABLE_GLOBAL_INTERRUPT();
        gasDPM[u8PortNum].u8PowerFaultISR |= u8FaultType;
        MCHP_PSF_HOOK_ENABLE_GLOBAL_INTERRUPT(); 
    }
} 
#endif 


/************************DPM Notification Handler ******************************/
UINT8 DPM_NotifyClient(UINT8 u8PortNum, eMCHP_PSF_NOTIFICATION eDPMNotification)
{
    UINT8 u8Return = TRUE; 
    
#if (TRUE == INCLUDE_POWER_BALANCING)
    if (TRUE == DPM_IS_PB_ENABLED(u8PortNum))
    {
        u8Return = PB_HandleDPMEvents(u8PortNum, eDPMNotification);
    }
#endif

    /* DPM notifications that need to be handled by stack applications must
       be added here before calling the user function. */
    switch(eDPMNotification)
    {
        case eMCHP_PSF_TYPEC_DETACH_EVENT:
        {
            DEBUG_PRINT_PORT_STR (u8PortNum,"***************TYPEC DETACH***********\r\n");
            /* Process Type C Detach Event */
            DPM_OnTypeCDetach(u8PortNum); 
            break;
        }
        case eMCHP_PSF_TYPEC_CC1_ATTACH:
        {
            DEBUG_PRINT_PORT_STR (u8PortNum,"***************TYPEC CC1 ATTACH***********\r\n");
            /* Update Attached and Orientation connection status */
            gasCfgStatusData.sPerPortData[u8PortNum].u32PortConnectStatus |= DPM_PORT_ATTACHED_STATUS;  
            gasCfgStatusData.sPerPortData[u8PortNum].u32PortConnectStatus &= 
                                                ~(DPM_PORT_ORIENTATION_FLIPPED_STATUS);            
            /* Assert Orientation LED */
            MCHP_PSF_HOOK_GPIO_FUNC_DRIVE(u8PortNum, eORIENTATION_FUNC, eGPIO_ASSERT);     
            break;
        }
        case eMCHP_PSF_TYPEC_CC2_ATTACH:
        {
            DEBUG_PRINT_PORT_STR (u8PortNum,"***************TYPEC CC2 ATTACH***********\r\n");
            /* Update Orientation connection status */
            gasCfgStatusData.sPerPortData[u8PortNum].u32PortConnectStatus |= 
                            (DPM_PORT_ATTACHED_STATUS | DPM_PORT_ORIENTATION_FLIPPED_STATUS);                        

            /* De-assert Orientation LED */
            MCHP_PSF_HOOK_GPIO_FUNC_DRIVE(u8PortNum, eORIENTATION_FUNC, eGPIO_DEASSERT);           
            break;
        }
        case eMCHP_PSF_CAPS_MISMATCH:
        {
            #if (TRUE == INCLUDE_PD_SINK)
            gasCfgStatusData.sPerPortData[u8PortNum].u32PortIOStatus |= DPM_PORT_IO_CAP_MISMATCH_STATUS;
            MCHP_PSF_HOOK_GPIO_FUNC_DRIVE(u8PortNum, eSNK_CAPS_MISMATCH_FUNC, eGPIO_ASSERT);
            #endif
            break;
        }
        case eMCHP_PSF_NEW_SRC_CAPS_RCVD:
        {
            #if (TRUE == INCLUDE_PD_SINK)
            gasCfgStatusData.sPerPortData[u8PortNum].u32PortIOStatus &=\
                    ~DPM_PORT_IO_CAP_MISMATCH_STATUS;
            MCHP_PSF_HOOK_GPIO_FUNC_DRIVE(u8PortNum, eSNK_CAPS_MISMATCH_FUNC, eGPIO_DEASSERT);
            #endif
            break;
        }
        case eMCHP_PSF_SINK_ALERT_RCVD:
        {
            #if (TRUE == INCLUDE_PD_SOURCE_PPS)
            /* Initiate transmission of Get_Status message on reception of Alert from partner */
            DPM_RegisterInternalEvent(u8PortNum, DPM_INT_EVT_INITIATE_GET_STATUS);
            #endif 
            break; 
        }         
        case eMCHP_PSF_PORT_DISABLED:
        {
            DPM_OnTypeCDetach(u8PortNum);
            break;
        }
        case eMCHP_PSF_VDM_RESPONSE_RCVD:
        case eMCHP_PSF_VDM_RESPONSE_NOT_RCVD:
        {
            /* Clear the VDM internal event since the AMS is complete */
            #if (TRUE == INCLUDE_PD_VDM)            
            gasDPM[u8PortNum].u16DPMInternalEvents &= ~(DPM_INT_EVT_INITIATE_VDM);
            #endif
            break; 
        }
        case eMCHP_PSF_VCONN_SWAP_COMPLETE:
        {
            #if (TRUE == INCLUDE_PD_VCONN_SWAP)
            if ((DPM_IGNORE_INITIATE_SWAP == DPM_EvaluateRoleSwap (u8PortNum, eVCONN_SWAP_INITIATE)) || \
                        (gasDPM[u8PortNum].u32DPMStatus & DPM_VCONN_SWAP_INIT_STS_AS_VCONNSRC))
            {
                /* Clear VCONN Swap internal event due to mismatch of policy */
                gasDPM[u8PortNum].u16DPMInternalEvents &= ~(DPM_INT_EVT_INITIATE_VCONN_SWAP);
            }
            #endif
            break; 
        }
        case eMCHP_PSF_PR_SWAP_COMPLETE:
        {
            #if (TRUE == INCLUDE_PD_PR_SWAP)
            if (DPM_IGNORE_INITIATE_SWAP == DPM_EvaluateRoleSwap (u8PortNum, ePR_SWAP_INITIATE))
            {
                /* Clear PR Swap internal event due to mismatch of policy */
                gasDPM[u8PortNum].u16DPMInternalEvents &= ~(DPM_INT_EVT_INITIATE_PR_SWAP);
            }
            #endif
            break; 
        }        
        case eMCHP_PSF_DR_SWAP_COMPLETE:
        {
            #if (TRUE == INCLUDE_PD_DR_SWAP)
            if (DPM_IGNORE_INITIATE_SWAP == DPM_EvaluateRoleSwap (u8PortNum, eDR_SWAP_INITIATE))
            {
                /* Clear DR Swap internal event due to mismatch of policy */
                gasDPM[u8PortNum].u16DPMInternalEvents &= ~(DPM_INT_EVT_INITIATE_DR_SWAP);
            }
            #endif
            break; 
        }         
        case eMCHP_PSF_HARD_RESET_COMPLETE:
        {
            DEBUG_PRINT_PORT_STR (u8PortNum,"***************HARD RESET COMPLETE***********\r\n");
            break; 
        } 
        case eMCHP_PSF_SINK_CAPS_RCVD:
        {
            #if (TRUE == CONFIG_HOOK_DEBUG_MSG)
            DPM_EvaluatePartnerCapabilities (u8PortNum);
            #endif 
            break; 
        }
        default:
            break;
    }
    
    /*Notify the application layer*/
    u8Return &= MCHP_PSF_NOTIFY_CALL_BACK(u8PortNum, eDPMNotification); 
    return u8Return;
}

/************************DPM Client Request Handling API ******************************/ 
void DPM_ClientRequestHandler(UINT8 u8PortNum)
{
    /* Check if at least one request is initiated by any application. This 
       check saves code execution time by letting the control not to check 
       for each if-else condition present inside in case this condition
       is false. */ 
    if (DPM_NO_CLIENT_REQ_PENDING == gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest)
    {
        return;
    }

    UINT8 u8CurrentPwrRole = DPM_GET_CURRENT_POWER_ROLE(u8PortNum);
    
    /* Check for Port enable/disable and VBUS Fault Handling requests. Policy Engine Idle check 
       is not needed for these requests and they have to be handled with highest priority*/
    if (DPM_CLIENT_REQ_PORT_DISABLE & gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest)
    {        
#if(TRUE == INCLUDE_PD_DRP)
        /*Disable DRP offload as soon as port disable client request is triggered.*/
        UPD_RegByteClearBit(u8PortNum, TYPEC_DRP_CTL_LOW, TYPEC_DRP_EN);
#endif
        /* Clear the client request since it is accepted */
        gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest &= 
                                    ~(DPM_CLIENT_REQ_PORT_DISABLE);
        
        /* Request DPM to disable port */
        DPM_RegisterInternalEvent(u8PortNum, DPM_INT_EVT_PORT_DISABLE);
    }
    else if (DPM_CLIENT_REQ_PORT_ENABLE & gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest)
    {
        /* Clear the client request since it is accepted */
        gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest &= 
                                    ~(DPM_CLIENT_REQ_PORT_ENABLE);
        
        /* Request DPM to enable port */
        DPM_RegisterInternalEvent(u8PortNum, DPM_INT_EVT_PORT_ENABLE);
    }
#if (TRUE == INCLUDE_POWER_FAULT_HANDLING)
    else if (DPM_CLIENT_REQ_HANDLE_FAULT_VBUS_OV & gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest)
    {
        /* Clear the client request since it is accepted */
        gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest &= 
                                    ~(DPM_CLIENT_REQ_HANDLE_FAULT_VBUS_OV);
                
        /* Call the DPM API that sets the VBUS OV flag*/
        DPM_HandleExternalVBUSFault(u8PortNum, DPM_POWER_FAULT_OVP); 
    }
    else if (DPM_CLIENT_REQ_HANDLE_FAULT_VBUS_OCS & gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest)
    {
        /* Clear the client request since it is accepted */
        gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest &=
                                    ~(DPM_CLIENT_REQ_HANDLE_FAULT_VBUS_OCS);
                
        /*Inform DPM to handle VBUS OCS only if it is Fixed supply else it is operating condition
          change in case of PPS supply*/
        #if (TRUE == INCLUDE_PD_SOURCE_PPS)
        if (DPM_PD_PPS_CONTRACT == DPM_GET_CURRENT_EXPLICIT_CONTRACT(u8PortNum))
        {
            /*Operating condition change  as Alert Type and OMF is set to Current Limit mode*/
            gasDPM[u8PortNum].u8AlertType |= DPM_ALERT_TYPE_OPR_COND_CHANGE;
            gasDPM[u8PortNum].u8StatusEventFlags |= DPM_EVENT_TYPE_CL_MODE;
            gasDPM[u8PortNum].u8RealTimeFlags |= DPM_REAL_TIME_FLAG_OMF_IN_CL_MODE;
            /*Initiate Alert message*/
            DPM_RegisterInternalEvent(u8PortNum, DPM_INT_EVT_INITIATE_ALERT);
            
        }
        else
        #endif /*INCLUDE_PD_SOURCE_PPS*/
        {
            /*Fixed supply*/
            /* Call the DPM API to handle external VBUS fault */
            DPM_HandleExternalVBUSFault(u8PortNum, DPM_POWER_FAULT_VBUS_OCS);
        }   
    }
    else if (DPM_CLIENT_REQ_HANDLE_VBUS_OCS_EXIT & gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest)
    {
        /* Clear the client request since it is accepted */
        gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest &=
                                ~(DPM_CLIENT_REQ_HANDLE_VBUS_OCS_EXIT);
                
        #if (TRUE == INCLUDE_PD_SOURCE_PPS)
        if (DPM_PD_PPS_CONTRACT == DPM_GET_CURRENT_EXPLICIT_CONTRACT(u8PortNum))
        {
            /*Operating condition change  as Alert Type and OMF is set to Current Limit mode*/
            gasDPM[u8PortNum].u8AlertType |= DPM_ALERT_TYPE_OPR_COND_CHANGE;
            gasDPM[u8PortNum].u8StatusEventFlags &= ~DPM_EVENT_TYPE_CL_CV_MODE_MASK;
            gasDPM[u8PortNum].u8RealTimeFlags &= ~DPM_REAL_TIME_FLAG_OMF_FIELD_MASK;
            /*Initiate Alert message*/
            DPM_RegisterInternalEvent(u8PortNum, DPM_INT_EVT_INITIATE_ALERT);
        }
        else
        {
            /*no need to handle of fixed supply*/
        }
        #endif
    }
#endif /*INCLUDE_POWER_FAULT_HANDLING*/       
#if (TRUE == INCLUDE_PD_ALT_MODE)
    else if (DPM_CLIENT_REQ_RESPOND_VDM & gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest)
    {
        /* Clear the request since the request is accepted and going to be handled */
        gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest &= 
                                  ~(DPM_CLIENT_REQ_RESPOND_VDM);
            
        /* Assigning the PE states to VDM states directly because this client 
           request is not the start of an AMS. So, no need to handle Collision 
           Avoidance. If we handle this client request via Internal Event Handler, 
           the VDM response would never be sent if the port power role is Sink 
           since the Rp value might be set as 3A by the Source partner. It is checked              
           whether PE is in Ready state before sending a response. Without this check, 
           if application raises this client request when some other AMS is in 
           progress, then it would corrupt the PE states */
        if ((gasPolicyEngine[u8PortNum].ePEState == ePE_SRC_READY) || \
                        (gasPolicyEngine[u8PortNum].ePEState == ePE_SNK_READY))
        {
            gasPolicyEngine[u8PortNum].ePEState = ePE_VDM_RESPOND_VDM; 
            gasPolicyEngine[u8PortNum].ePESubState = ePE_VDM_RESPOND_VDM_SVID_SPECIFIC_SS;                                            
        }
    } /* DPM_CLIENT_REQ_RESPOND_VDM */  
#endif     
    /* Check for renegotiation request */
    else if (DPM_CLIENT_REQ_RENEGOTIATE & gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest)
    {
        /* Clear the request since the request is accepted and going to be handled */
        gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest &= 
                                      ~(DPM_CLIENT_REQ_RENEGOTIATE);  

        /* Request DPM for renegotiation */
        DPM_RegisterInternalEvent(u8PortNum, DPM_INT_EVT_INITIATE_RENEGOTIATION);

#if (TRUE == INCLUDE_PD_FR_SWAP)        
        /*Disable FRS_REQ_PIO/FRS_DET_EN as power transition due to this renegotiation might affect FRS*/
        if (PD_ROLE_SOURCE == u8CurrentPwrRole)
        {
            DPM_DISABLE_FRS_REQ_PIO(u8PortNum);
            DEBUG_PRINT_PORT_STR(u8PortNum, "FRS_REQ_PIO Disabled\r\n");
        }
        else
        {
            DPM_DISABLE_FRS_DET_EN(u8PortNum);
            DEBUG_PRINT_PORT_STR(u8PortNum, "FRS_DET_EN Disabled\r\n");
        }
#endif                                 
    } /*DPM_CLIENT_REQ_RENEGOTIATE*/    
#if (TRUE == INCLUDE_PD_VCONN_SWAP)
        /* Check for DPM_CLIENT_REQ_VCONN_SWAP request */
    else if (DPM_CLIENT_REQ_VCONN_SWAP & gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest)
    {
        /* Clear the request since the request is accepted and going to be handled */
        gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest &= 
                                      ~(DPM_CLIENT_REQ_VCONN_SWAP);   
        
        /* Request DPM for VCONN swap */
        DPM_RegisterInternalEvent(u8PortNum, DPM_INT_EVT_INITIATE_VCONN_SWAP);
    } /*DPM_CLIENT_REQ_VCONN_SWAP*/
#endif     
#if (TRUE == INCLUDE_PD_PR_SWAP)
    /* Check for DPM_CLIENT_REQ_PR_SWAP request */
    else if (DPM_CLIENT_REQ_PR_SWAP & gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest)
    {   
        /* Clear the request since the request is accepted and going to be handled */
        gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest &= 
                                      ~(DPM_CLIENT_REQ_PR_SWAP); 
        
        /* Request DPM for PR swap */
        DPM_RegisterInternalEvent(u8PortNum, DPM_INT_EVT_INITIATE_PR_SWAP);
        
#if (TRUE == INCLUDE_PD_FR_SWAP)        
        /*Disable FRS_REQ_PIO/FRS_DET_EN to avoid FRS between a PR_Swap */
        if (PD_ROLE_SOURCE == u8CurrentPwrRole)
        {
            DPM_DISABLE_FRS_REQ_PIO(u8PortNum);
            DEBUG_PRINT_PORT_STR(u8PortNum, "FRS_REQ_PIO Disabled\r\n");
        }
        else
        {
            DPM_DISABLE_FRS_DET_EN(u8PortNum);
            DEBUG_PRINT_PORT_STR(u8PortNum, "FRS_DET_EN Disabled\r\n");
        }
#endif                 
    } /*DPM_CLIENT_REQ_PR_SWAP*/
#endif     
#if (TRUE == INCLUDE_PD_DR_SWAP)
    /* Check for DPM_CLIENT_REQ_DR_SWAP request */
    else if (DPM_CLIENT_REQ_DR_SWAP & gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest)
    {            
        /* Clear the request since the request is accepted and going to be handled */
        gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest &= 
                                      ~(DPM_CLIENT_REQ_DR_SWAP); 
        
        /* Request DPM for DR swap */
        DPM_RegisterInternalEvent(u8PortNum, DPM_INT_EVT_INITIATE_DR_SWAP);
    } /*DPM_CLIENT_REQ_DR_SWAP*/
#endif     
#if (TRUE == INCLUDE_PD_VDM)
    else if (DPM_CLIENT_REQ_INITIATE_VDM & gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest)
    {
        /* Clear the request since the request is accepted and going to be handled */
        gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest &= 
                                      ~(DPM_CLIENT_REQ_INITIATE_VDM);

        /* Request DPM for initiating a VDM request */
        DPM_RegisterInternalEvent(u8PortNum, DPM_INT_EVT_INITIATE_VDM);                                    
    }  /* DPM_CLIENT_REQ_INITIATE_VDM */
#endif
#if(TRUE == INCLUDE_UPD_HPD)
    else if(DPM_CLIENT_REQ_DISABLE_HPD & gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest)
    {
        /* Clear the request since the request is accepted and going to be handled */
        gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest &= 
                                      ~(DPM_CLIENT_REQ_DISABLE_HPD);
        
        UPD_RegByteClearBit (u8PortNum, UPD_HPD_CTL, UPD_HPD_ENABLE); 
        gu8HPDNextIndex[u8PortNum] = SET_TO_ZERO; 
        gu16HPDStsISR[u8PortNum] = SET_TO_ZERO;
                                      
        DEBUG_PRINT_PORT_STR(u8PortNum, "UPD_HPD Disabled\r\n");
        
        (void)DPM_NotifyClient(u8PortNum, eMCHP_PSF_HPD_DISABLED);
    }
    else if(DPM_CLIENT_REQ_ENABLE_HPD & gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest)
    {
        /* Clear the request since the request is accepted and going to be handled */
        gasCfgStatusData.sPerPortData[u8PortNum].u32ClientRequest &= 
                                      ~(DPM_CLIENT_REQ_ENABLE_HPD);
        
        DPM_ENABLE_HPD(u8PortNum);
        DEBUG_PRINT_PORT_STR(u8PortNum, "UPD_HPD Enabled\r\n");

        (void)DPM_NotifyClient(u8PortNum, eMCHP_PSF_HPD_ENABLED);        
    }
#endif
    else
    {
        /* Do Nothing */
    } 
}

/************************DPM Internal Event Handling APIs *******************************/
void DPM_RegisterInternalEvent(UINT8 u8PortNum, UINT16 u16EventType)
{
    gasDPM[u8PortNum].u16DPMInternalEvents |= u16EventType;
}

void DPM_InternalEventHandler(UINT8 u8PortNum)
{
    UINT16 u16AMSInProgress = SET_TO_ZERO;
    UINT8 u8DPMPowerRole = DPM_GET_CURRENT_POWER_ROLE(u8PortNum);
    
    /*Process port disable/enable internal events irrespective of whether PSF is idle*/
    if(DPM_INT_EVT_PORT_DISABLE == (gasDPM[u8PortNum].u16DPMInternalEvents &\
                                                DPM_INT_EVT_PORT_DISABLE))
    {
        /*Clear the Internal event since it is processed*/
        gasDPM[u8PortNum].u16DPMInternalEvents &= ~(DPM_INT_EVT_PORT_DISABLE);

        DPM_EnablePort(u8PortNum, FALSE);
    }
    else if(DPM_INT_EVT_PORT_ENABLE == (gasDPM[u8PortNum].u16DPMInternalEvents &\
                                                DPM_INT_EVT_PORT_ENABLE))
    {
        /*Clear the Internal event since it is processed*/
        gasDPM[u8PortNum].u16DPMInternalEvents &= ~(DPM_INT_EVT_PORT_ENABLE);

        DPM_EnablePort(u8PortNum, TRUE);
    }
#if (TRUE == INCLUDE_PD_FR_SWAP)
    else if ((gasDPM[u8PortNum].u16DPMInternalEvents & DPM_INT_EVT_INITIATE_FR_SWAP) &&\
                PRL_IsAMSInitiatable(u8PortNum))
    {
        /*Clear the Internal event since it is processed*/
        gasDPM[u8PortNum].u16DPMInternalEvents &= ~(DPM_INT_EVT_INITIATE_FR_SWAP);
                
        gasPolicyEngine[u8PortNum].ePEState = ePE_FRS_SNK_SRC_START_AMS;
        u16AMSInProgress = DPM_INT_EVT_INITIATE_FR_SWAP;
        DEBUG_PRINT_PORT_STR (u8PortNum,"DPM: FR_SWAP INITIATED\r\n");
        /* To-do: This process can occur at any time, even during a Non-interruptible AMS in 
           which case error handling such as Hard Reset or [USB Type-C 2.0] Error Recovery will be triggered.
           if (PE_IsPolicyEngineIdle(u8PortNum))
           {
                // Change the PE state to send FR Swap message
           }
           else
           {
               // Send Hard Reset
           }
         */
    }
#endif 
#if (TRUE == INCLUDE_PD_3_0)
    /* Process internal events only when the Policy Engine is in PS_RDY state*/
    else if ((gasDPM[u8PortNum].u16DPMInternalEvents) && (TRUE == PE_IsPolicyEngineIdle(u8PortNum)) &&\
            PRL_IsAMSInitiatable(u8PortNum))
    {
#else
    else if ((gasDPM[u8PortNum].u16DPMInternalEvents) && (TRUE == PE_IsPolicyEngineIdle(u8PortNum)))
    {      
#endif   
        /*If condition is ordered based on the internal event priority*/
        if (DPM_INT_EVT_INITIATE_GET_SINK_CAPS == (gasDPM[u8PortNum].u16DPMInternalEvents &\
                                                    DPM_INT_EVT_INITIATE_GET_SINK_CAPS))
        {
            /*Clear the Internal event since it is processed*/
            gasDPM[u8PortNum].u16DPMInternalEvents &= ~(DPM_INT_EVT_INITIATE_GET_SINK_CAPS);
            
            /* Move the Policy Engine to PE_GET_SINK_CAP state */
            gasPolicyEngine[u8PortNum].ePEState = ePE_GET_SINK_CAP; 
            gasPolicyEngine[u8PortNum].ePESubState = ePE_GET_SINK_CAP_ENTRY_SS;
            u16AMSInProgress = DPM_INT_EVT_INITIATE_GET_SINK_CAPS;
            DEBUG_PRINT_PORT_STR (u8PortNum,"DPM: GET_SINK_CAP INITIATED\r\n");
        }
        else if (DPM_INT_EVT_INITIATE_RENEGOTIATION == (gasDPM[u8PortNum].u16DPMInternalEvents &\
                                                    DPM_INT_EVT_INITIATE_RENEGOTIATION))
        {
            /*Clear the Internal event since it is processed*/
            gasDPM[u8PortNum].u16DPMInternalEvents &= ~(DPM_INT_EVT_INITIATE_RENEGOTIATION);

            /* Check for Port Power Role */
            if (PD_ROLE_SOURCE == u8DPMPowerRole)
            {
                /* Move the Policy Engine to PE_SRC_SEND_CAPABILITIES state */
                gasPolicyEngine[u8PortNum].ePEState = ePE_SRC_SEND_CAPABILITIES;
                gasPolicyEngine[u8PortNum].ePESubState = ePE_SRC_SEND_CAP_ENTRY_SS;
            }
            else
            {
                gasPolicyEngine[u8PortNum].ePEState = ePE_SNK_EVALUATE_CAPABILITY;
            }
            u16AMSInProgress = DPM_INT_EVT_INITIATE_RENEGOTIATION;
            DEBUG_PRINT_PORT_STR (u8PortNum,"DPM: RENEGOTIATION INITIATED\r\n");
        }
#if (TRUE == INCLUDE_PD_VCONN_SWAP)
        else if (DPM_INT_EVT_INITIATE_VCONN_SWAP == (gasDPM[u8PortNum].u16DPMInternalEvents &\
                                                    DPM_INT_EVT_INITIATE_VCONN_SWAP))
        {            
            /*Clear the Internal event since it is processed*/
            gasDPM[u8PortNum].u16DPMInternalEvents &= ~(DPM_INT_EVT_INITIATE_VCONN_SWAP);
            
            if (DPM_REQUEST_SWAP == DPM_EvaluateRoleSwap (u8PortNum, eVCONN_SWAP_INITIATE))
            {
                gasPolicyEngine[u8PortNum].ePEState = ePE_VCS_SEND_SWAP;
                gasPolicyEngine[u8PortNum].ePESubState = ePE_VCS_SEND_SWAP_ENTRY_SS;
                u16AMSInProgress = DPM_INT_EVT_INITIATE_VCONN_SWAP;
                DEBUG_PRINT_PORT_STR (u8PortNum,"DPM: VCONN_SWAP INITIATED\r\n");
            }  
        }
#endif /*INCLUDE_PD_VCONN_SWAP*/
#if (TRUE == INCLUDE_PD_PR_SWAP)
        else if (DPM_INT_EVT_INITIATE_PR_SWAP == (gasDPM[u8PortNum].u16DPMInternalEvents &\
                                                    DPM_INT_EVT_INITIATE_PR_SWAP))
        { 
            UINT32 u32PartnerPDO = SET_TO_ZERO;
            
            /*Clear the Internal event since it is processed*/
            gasDPM[u8PortNum].u16DPMInternalEvents &= ~(DPM_INT_EVT_INITIATE_PR_SWAP);
            
           
            /*Process initiate PR_SWAP only if the port partner and PSF port supports dual
              power and PR_SWAP initiation still valid as per the current power role*/
            if ((DPM_REQUEST_SWAP == DPM_EvaluateRoleSwap (u8PortNum, ePR_SWAP_INITIATE)) &&\
                    (DPM_GET_PDO_DUAL_POWER(gasCfgStatusData.sPerPortData[u8PortNum].u32aAdvertisedPDO[INDEX_0])))
            {
                if(PD_ROLE_DRP == u8DPMPowerRole)
                {
                    /*Execution is not expected to hit here*/
                    /*Do nothing*/
                }
                else if(PD_ROLE_SOURCE == u8DPMPowerRole)
                {
                    u32PartnerPDO = gasCfgStatusData.sPerPortData[u8PortNum].u32aPartnerSinkPDO[INDEX_0];
                }
                else
                {
                    u32PartnerPDO = gasCfgStatusData.sPerPortData[u8PortNum].u32aPartnerSourcePDO[INDEX_0];
                }
                
                if(DPM_GET_PDO_DUAL_POWER(u32PartnerPDO))
                {
                    gasPolicyEngine[u8PortNum].ePEState = ePE_PRS_SEND_SWAP;
                    gasPolicyEngine[u8PortNum].ePESubState = ePE_PRS_SEND_SWAP_ENTRY_SS;
                    u16AMSInProgress = DPM_INT_EVT_INITIATE_PR_SWAP;
                    DEBUG_PRINT_PORT_STR (u8PortNum,"DPM: PR_SWAP INITIATED\r\n");
                }
            }
        }
#endif /*INCLUDE_PD_PR_SWAP*/
#if (TRUE == INCLUDE_PD_DR_SWAP)
        else if (DPM_INT_EVT_INITIATE_DR_SWAP == (gasDPM[u8PortNum].u16DPMInternalEvents &\
                                                    DPM_INT_EVT_INITIATE_DR_SWAP))
        {
            UINT32 u32PartnerPDO = SET_TO_ZERO;
            
            /*Clear the Internal event since it is processed*/
            gasDPM[u8PortNum].u16DPMInternalEvents &= ~(DPM_INT_EVT_INITIATE_DR_SWAP);
            
            /*Check whether it still valid to initiate a swap and the partner supports dual role data
              and the port is not in modal operation */
            if ((DPM_REQUEST_SWAP == DPM_EvaluateRoleSwap (u8PortNum, eDR_SWAP_INITIATE)) &&\
                    (DPM_GET_PDO_DUAL_DATA(gasCfgStatusData.sPerPortData[u8PortNum].u32aAdvertisedPDO[INDEX_0])) &&\
                            (!(gasDPM[u8PortNum].u32DPMStatus & DPM_PORT_IN_MODAL_OPERATION)))                     
            {
                if(PD_ROLE_DRP == u8DPMPowerRole)
                {
                    /*Execution is not expected to hit here*/
                    /*Do nothing*/
                }
                else if(PD_ROLE_SOURCE == u8DPMPowerRole)
                {
                    u32PartnerPDO = gasCfgStatusData.sPerPortData[u8PortNum].u32aPartnerSinkPDO[INDEX_0];
                }
                else
                {
                    u32PartnerPDO = gasCfgStatusData.sPerPortData[u8PortNum].u32aPartnerSourcePDO[INDEX_0];
                }
                
                if(DPM_GET_PDO_DUAL_DATA(u32PartnerPDO))
                {
                    gasPolicyEngine[u8PortNum].ePEState = ePE_DRS_SEND_SWAP;
                    gasPolicyEngine[u8PortNum].ePESubState = ePE_DRS_SEND_SWAP_ENTRY_SS;
                    u16AMSInProgress = DPM_INT_EVT_INITIATE_DR_SWAP;  
                    DEBUG_PRINT_PORT_STR (u8PortNum,"DPM: DR_SWAP INITIATED\r\n");
                }
            }
        }
#endif/*INCLUDE_PD_DR_SWAP*/
#if (TRUE == INCLUDE_PD_VDM)
        else if (DPM_INT_EVT_INITIATE_VDM == (gasDPM[u8PortNum].u16DPMInternalEvents &\
                                                    DPM_INT_EVT_INITIATE_VDM))
        {
            /* Internal event will be cleared once the VDM AMS is complete.
               No need to do it here */
            
            /* Move the Policy Engine to ePE_INIT_PORT_VDM_IDENTITY_REQUEST state */
            gasPolicyEngine[u8PortNum].ePEState = ePE_VDM_INITIATE_VDM; 
            gasPolicyEngine[u8PortNum].ePESubState = ePE_VDM_INITIATE_VDM_ENTRY_SS;
            u16AMSInProgress = DPM_INT_EVT_INITIATE_VDM;            
        }
#endif /* INCLUDE_PD_VDM */
#if (TRUE == INCLUDE_PD_SOURCE_PPS)
        else if (DPM_INT_EVT_INITIATE_ALERT == (gasDPM[u8PortNum].u16DPMInternalEvents & DPM_INT_EVT_INITIATE_ALERT))
        {     
            /*Clear the Internal event since it is processed*/
            gasDPM[u8PortNum].u16DPMInternalEvents &= ~(DPM_INT_EVT_INITIATE_ALERT);

            /* Process Alert transmission only if current PD Spec rev is 3.0, 
               a PPS APDO is advertised and at least one of the bits must be 
               set in TypeOfAlert field */
            if ((PD_SPEC_REVISION_3_0 == DPM_GET_CURRENT_PD_SPEC_REV(u8PortNum)) &&\
                        (TRUE == DPM_IsAPDOAdvertised(u8PortNum)) &&\
                        (gasDPM[u8PortNum].u8AlertType != SET_TO_ZERO))
            {
                if (PD_ROLE_SOURCE == u8DPMPowerRole)
                {
                    /* Move the Policy Engine to ePE_SRC_SEND_SOURCE_ALERT state */
                    gasPolicyEngine[u8PortNum].ePEState = ePE_SRC_SEND_SOURCE_ALERT; 
                    gasPolicyEngine[u8PortNum].ePESubState = ePE_SRC_SEND_SOURCE_ALERT_ENTRY_SS;
                    u16AMSInProgress = DPM_INT_EVT_INITIATE_ALERT;
                }
                else
                {
                    /* Do nothing for sink as PPS sink not supported currently*/
                }                
            }

            /*start the DPM_STATUS_FAULT_PERSIST_TIMEOUT_MS to clear the status flag
              on timeout if there is no request for status*/
            gasDPM[u8PortNum].u8StsClearTmrID = PDTimer_Start (DPM_STATUS_FAULT_PERSIST_TIMEOUT_MS,\
                                                      DPM_StatusFaultPersist_TimerCB, u8PortNum, (UINT8)SET_TO_ZERO);

        } /* DPM_INT_EVT_INITIATE_ALERT */ 
        else if (DPM_INT_EVT_INITIATE_GET_STATUS == (gasDPM[u8PortNum].u16DPMInternalEvents\
                                                    & DPM_INT_EVT_INITIATE_GET_STATUS))
        {
            /*Clear the Internal event since it is processed*/
            gasDPM[u8PortNum].u16DPMInternalEvents &= ~(DPM_INT_EVT_INITIATE_GET_STATUS);

            /* Process Get_Status transmission only if current PD Spec rev is 3.0, 
               a PPS APDO is advertised */            
            if ((PD_SPEC_REVISION_3_0 == DPM_GET_CURRENT_PD_SPEC_REV(u8PortNum)) &&\
                        (TRUE == DPM_IsAPDOAdvertised(u8PortNum)))
            {
                if (PD_ROLE_SOURCE == u8DPMPowerRole)
                {
                    /* Move the Policy Engine to ePE_SRC_GET_SINK_STATUS state */
                    gasPolicyEngine[u8PortNum].ePEState = ePE_SRC_GET_SINK_STATUS; 
                    gasPolicyEngine[u8PortNum].ePESubState = ePE_SRC_GET_SINK_STATUS_ENTRY_SS;
                    u16AMSInProgress = DPM_INT_EVT_INITIATE_GET_STATUS;
                }
                else
                {
                    /*Do nothing for sink as PPS for sink not supported currently*/
                }
            }
        } /* DPM_INT_EVT_INITIATE_GET_STATUS */
#endif /*INCLUDE_PD_SOURCE_PPS*/
        else
        {
            /* Do Nothing */
        }          
    }
    
    if (u16AMSInProgress)
    {                
		gasDPM[u8PortNum].u16InternalEvntInProgress = u16AMSInProgress;
        #if (TRUE == INCLUDE_PD_3_0)
        if (PD_ROLE_SOURCE == u8DPMPowerRole)
        {
            PRL_SetCollisionAvoidance (u8PortNum, TYPEC_SINK_TXNG);   
        }
        #endif
        /*Irrespective of the Role indicate DPM AMS is initiated with CA*/
        MCHP_PSF_HOOK_DISABLE_GLOBAL_INTERRUPT();
        gasPRL[u8PortNum].u8TxStsDPMSyncISR = TRUE;
        MCHP_PSF_HOOK_ENABLE_GLOBAL_INTERRUPT();           
    }
}

#if (TRUE == INCLUDE_PD_ALT_MODE) 
    
void DPM_AltModeEventHandler(UINT8 u8PortNum)
{    
#if (TRUE == INCLUDE_UPD_HPD)    
    UINT16 u16HPDStsISR;
    UINT8 u8HPDCurrentIndex = gu8HPDNextIndex[u8PortNum];
    UINT8 u8Data;
    
    MCHP_PSF_HOOK_DISABLE_GLOBAL_INTERRUPT();
    u16HPDStsISR = gu16HPDStsISR[u8PortNum];
    gu16HPDStsISR[u8PortNum] &= (~UPD_HPD_INTERRUPT_OCCURRED);
    MCHP_PSF_HOOK_ENABLE_GLOBAL_INTERRUPT();
    
    if(u16HPDStsISR & UPD_HPD_INTERRUPT_OCCURRED)
    {
        /*Lower byte of u16HPDStsISR is copied to u8Data*/
        u8Data = u16HPDStsISR;
        
        for(UINT8 u8QueueEvtNo = SET_TO_ZERO; u8QueueEvtNo < UPD_HPD_QUEUE_SIZE; u8QueueEvtNo++)
       {
           UINT8 u8QueueEntry = ((u8Data >> (UPD_HPD_EVENT_SIZE*u8HPDCurrentIndex)) & UPD_HPD_EVENT_MASK);
           if(u8QueueEntry)
           {
               u8HPDCurrentIndex++;
               u8HPDCurrentIndex = (u8HPDCurrentIndex % UPD_HPD_QUEUE_SIZE);
               switch(u8QueueEntry)
               { 
                   case eMCHP_PSF_UPD_HPD_HIGH: 
                       (void)DPM_NotifyClient (u8PortNum, eMCHP_PSF_HPD_EVENT_HIGH); 
                       break; 
                   case eMCHP_PSF_UPD_HPD_LOW: 
                       (void)DPM_NotifyClient (u8PortNum, eMCHP_PSF_HPD_EVENT_LOW); 
                       break; 
                   case eMCHP_PSF_UPD_IRQ_HPD: 
                       (void)DPM_NotifyClient (u8PortNum, eMCHP_PSF_HPD_EVENT_IRQ_HPD); 
                       break; 
                   default: 
                       break; 
               }
           }
           else
           { 
               break; 
           }
       }
       gu8HPDNextIndex[u8PortNum] = u8HPDCurrentIndex;
    }
#endif 
    
    /* Post eMCHP_PSF_ALT_MODE_ENTRY_FAILED notification */
    if (gasDPM[u8PortNum].u32DPMStatus & DPM_AME_TIMER_DONE)
    {
        gasDPM[u8PortNum].u32DPMStatus &= ~(DPM_AME_TIMER_DONE);
        
        (void)DPM_NotifyClient(u8PortNum, eMCHP_PSF_ALT_MODE_ENTRY_FAILED);        
    }
}    

#endif
/*************************************************************************************/
