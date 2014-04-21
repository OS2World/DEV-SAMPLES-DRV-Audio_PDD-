//
// event.c
// 27-Jan-99
//

#include "cs40.h"




#if 0

/**@internal Report
 * @param    None
 * @return   None
 * @notes
 * This function will report (return) an expired event to SHDD
 */
void EVENT::Report(ULONG time)
{
    ULONG ulPopTime;

    // update the streamtime in the event report msg
   shdre.ulStreamTime = time;

   // set ulNextTime to -1 incase MMPM comes back in on
   // this thread and clobbers this event
   ulPopTime = ulNextTime;
   ulNextTime = 0xFFFFFFFF;

   // do an event trace
   pTrace->vLog(EVENT_RETURNED,he,time);

   // send the event back to the SHDD
   pstream->pfnSHD(&shdre);

      // if ulNextTime is still -1 then
      // we can see if we need to recalculate  ulNextTime
   if (ulNextTime == 0xFFFFFFFF) {
      // if this is a recurring event re-calculate ulNextTime
      if (ulFlags) {
         ulNextTime = ulRepeatTime + ulPopTime;
         shdre.ulStreamTime = ulNextTime;
      }
   }
   // tell the stream to find the next event to time out
   pstream->SetNextEvent();
}

HEVENT EVENT::GetHandle(void)
{
   return(he);
}

ULONG  EVENT::GetEventTime(void)
{
   return(ulNextTime);
}
/**@internal UpdateEvent
 * @param    PSTREAM        the address of the stream that owns this event
 * @param    HEVENT         the handle of this event
 * @param    PCONTROL_PARM  the address of the control parm associated with
 *                          this event
 * @return   None
 * @notes
 * "Updates" the event info when MMPM sends an Enable Event DDCMD and
 * the event has already been created.
 * Upon entry to this member function we could be touching values that
 * the interrupt handler may look at so we will cli/sti around the whole thing
 */
void EVENT::UpdateEvent(PSTREAM ps, HEVENT hevent, PCONTROL_PARM pcp)
{

   _cli_();
   pstream=ps;
   he=hevent;
   ULONG currenttime;

   currenttime = ps->GetCurrentTime();
   ulFlags = pcp->evcb.ulFlags & EVENT_RECURRING;

   if (ulFlags)
      ulRepeatTime = pcp->ulTime - currenttime;

   ulNextTime = pcp->ulTime;
   shdre.ulFunction = SHD_REPORT_EVENT;
   shdre.hStream = pstream->hstream;
   shdre.hEvent = he;
   shdre.ulStreamTime = ulNextTime;
   _sti_();
   pTrace->vLog(DDCMD_EnableEvent,hevent,ulRepeatTime,pcp->evcb.ulType,
                pcp->evcb.ulFlags, ulNextTime);

}
/**@internal EVENT
 * @param    PSTREAM        the address of the stream that owns this event
 * @param    HEVENT         the handle of this event
 * @param    PCONTROL_PARM  the address of the control parm associated with
 *                          this event
 * @return   None
 * @notes
 * the event class constructor
 */
EVENT::EVENT(PSTREAM ps, HEVENT hevent, PCONTROL_PARM pcp)
{
   UpdateEvent(ps, hevent, pcp);
   ps->qhEvent.PushOnTail((PQUEUEELEMENT)this);
}
/**@internal FindEvent
 * @param    HEVENT  event handle the caller is looking for
 * @param    PQUEUEHEAD the pointer the QUEUEHEAD for this EVENT
 * @return   PEVENT  the pointer to the event in question
 * @return   NULL    NULL if EVENT is not found
 * @notes
 * Globally scopped function that returns the pointer to a particular
 * event on the event queue
 */
PEVENT FindEvent(HEVENT he, PQUEUEHEAD pQH)
{
   PQUEUEELEMENT pqe = pQH->Head();

   while (pqe) {
      if (((PEVENT)pqe)->GetHandle() == he)
         return (PEVENT)pqe;
      pqe = pqe->pNext;
   }
   return NULL;
}
#endif
