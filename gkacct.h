/*
 * gkacct.h
 *
 * Accounting modules for GNU Gatekeeper. Provides generic
 * support for accounting to the gatekeeper.
 *
 * Copyright (c) 2003, Quarcom FHU, Michal Zygmuntowicz
 *
 * This work is published under the GNU Public License (GPL)
 * see file COPYING for details.
 * We also explicitely grant the right to link this code
 * with the OpenH323 library.
 *
 * $Log$
 * Revision 1.1.2.1  2003/06/19 15:36:04  zvision
 * Initial generic accounting support for GNU GK.
 *
 */
#ifndef __GKACCT_H_
#define __GKACCT_H_

#ifndef NAME_H
#include "name.h"
#endif
#ifndef SLIST_H
#include "slist.h"
#endif

/** Module for logging accounting events
	generated by the gatekeeper.
*/
class GkAcctLogger : public SList<GkAcctLogger>, public CNamedObject
{
public:
	/// Processing type for this module
	enum Control 
	{
		/// if cannot log an accounting event
		/// silently ignore the module and process remaining acct modules
		Optional, 
		/// if cannot log an accounting event do not allow futher 
		/// call processing (e.g. call should not be connected, etc.)
		/// always process remaining acct modules
		Required,
		/// if cannot log an accounting event
		/// stop processing and do not allow futher call progress
		/// (e.g. call should not be connected, etc.)
		/// if the event has been logged, do not process remaining acct modules
		Sufficient
	};

	/// status of the acct event processing
	enum Status 
	{
		Ok = 1,		/// acct even has been logged
		Fail = -1,	/// acct even has not been logged (failure)
		Next = 0	/// acct even has not been logged (undetermined status)
	};

	/// accounting event types
	enum AcctEvent 
	{
		AcctStart = 0x0001, /// start accounting a call
		AcctStop = 0x0002, /// stop accounting a call
		AcctUpdate = 0x0004, /// interim update
		AcctOn = 0x0008, /// accounting enabled (GK start)
		AcctOff = 0x0010, /// accounting disabled (GK stop)
		AcctAll = 0xffff,
		AcctNone = 0
	};
	
	/// Construct new accounting logger object.
	GkAcctLogger(
		/// module name (should be unique among different module types)
		const char* moduleName
		);
		
	virtual ~GkAcctLogger();

	/** @return
		Control flag determining processing behaviour for this module
		(optional,sufficient,required).
	*/
	Control GetControlFlag() const { return controlFlag; }

	/** @return
		Flags signalling which accounting events (see #AcctEvent enum#)
		should be logged. The flags are ORed.
	*/
	int GetEventMask() const { return eventMask; }
	
	/** Log an accounting event with this logger and loggers
		that following this one on the list.
	
		@return
		true if the event has been logged successfully, false otherwise.
	*/
	virtual bool LogAcctEvent( 
		AcctEvent evt, /// accounting event to log
		callptr& call /// additional data for the event
		);

	/** Format time string suitable for accounting logs.
		Currently NTP format conversion occurs.
		
		@return
		Formatted time string.
	*/ 
	static PString AsString( 
		const PTime& tm /// time value to be formatted
		);
	
	/** Format time string suitable for accounting logs.
		Currently NTP format conversion occurs.
		
		@return
		Formatted time string.
	*/ 
	static PString AsString( 
		const time_t& tm /// time value to be formatted
		);

protected:
	/** @return
		Default status that should be returned when the request
		cannot be determined.
	*/
	Status GetDefaultStatus() const { return defaultStatus; }

	/** @return
		A pointer to configuration settings for this logger.
	*/
	PConfig* GetConfig() const { return config; }
	
	/** Read an event mask (ORed #AccEvent enum# constants) 
		from the passed tokens.
	
		@return
		Resulting event mask.
	*/
	virtual int ReadEventMask( 
		const PStringArray& tokens /// event names (start from index 1)
		) const;

	/** Log an accounting event with this logger.
	
		@return
		Status of this logging operation (see #Status enum#)
	*/
	virtual Status Log(		
		AcctEvent evt, /// accounting event to log
		callptr& call /// additional data for the event
		);
		
private:
	GkAcctLogger(const GkAcctLogger &);
	GkAcctLogger & operator=(const GkAcctLogger &);
	 
private:
	/// processing behaviour (see #Control enum#)
	Control controlFlag;
	/// default processing status (see #Status enum#)
	Status defaultStatus;
	/// ORed #AcctEvent enum# constants - define which events are logged
	int eventMask;
	/// module settings
	PConfig* config;
};


/// Factory for instances of GkAcctLogger-derived classes
template<class Acct>
struct GkAcctLoggerCreator : public Factory<GkAcctLogger>::Creator0 {
	GkAcctLoggerCreator( const char* moduleName ) : Factory<GkAcctLogger>::Creator0(moduleName) {}
	virtual GkAcctLogger* operator()() const { return new Acct(m_id); }	
};

class GkAcctLoggerList 
{
public:
	/** Construct an empty list of accounting loggers.
	*/
	GkAcctLoggerList();
	
	/** Destroy the list of accounting loggers.
	*/
	~GkAcctLoggerList();

	/** Apply configuration changes to the list of accounting loggers.
		Usually it means destroying the old list and creating a new one.
	*/	
	void OnReload();
	
	/** Log accounting event with all active accounting loggers.
	
		@return
		true if the event has been successfully logged, false otherwise.
	*/
	bool LogAcctEvent( 
		GkAcctLogger::AcctEvent evt, /// the accounting event to be logged
		callptr& call /// a call associated with the event (if any)
		) 
	{
		ReadLock lock(m_reloadMutex);
		return !m_head || m_head->LogAcctEvent(evt,call);
	}
	
private:
	/// head of the accounting loggers list
	GkAcctLogger* m_head;
	/// mutex for thread-safe reload operation
	PReadWriteMutex m_reloadMutex;	
	
	GkAcctLoggerList(const GkAcctLogger&);
	GkAcctLoggerList& operator=(const GkAcctLoggerList&);
};

#endif  /* __GKACCT_H_ */
