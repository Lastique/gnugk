//////////////////////////////////////////////////////////////////
//
// Toolkit base class for the GnuGK
//
// This work is published under the GNU Public License (GPL)
// see file COPYING for details.
// We also explicitely grant the right to link this code
// with the OpenH323 library.
//
// History:
// 	991227  initial version (Torsten Will, mediaWays)
//
//////////////////////////////////////////////////////////////////

#ifndef TOOLKIT_H
#define TOOLKIT_H "@(#) $Id$"

#include <ptlib.h>
#include <ptlib/sockets.h>
#include <vector>
#include "singleton.h"

class H225_AliasAddress;
class H225_ArrayOf_AliasAddress;
class H225_H221NonStandard;

class Toolkit : public Singleton<Toolkit>
{
 public:
	// con- and destructing
	explicit Toolkit();
	virtual ~Toolkit();

	/// returns #basic# for
	virtual const PString GetName() const { return "basic"; }

	// by cwhuang
	// The idea was got from OpenGatekeeper,
	// but entirely implemented from scratch. :)
	class RouteTable {
		typedef PIPSocket::Address Address;
		typedef PIPSocket::InterfaceTable InterfaceTable;

	public:
		RouteTable() : rtable_begin(0) { /* initialize later */ }
		virtual ~RouteTable() { ClearTable(); }
		Address GetLocalAddress() const { return defAddr; };
		Address GetLocalAddress(const Address &) const;

		void InitTable();
		void ClearTable();
		bool IsEmpty() const { return rtable_begin == 0; }

	protected:
		class RouteEntry : public PIPSocket::RouteEntry {
		public:
#ifndef WIN32
			PCLASSINFO( RouteEntry, PIPSocket::RouteEntry )
#endif
			RouteEntry(const PString &);
			RouteEntry(const PIPSocket::RouteEntry &, const InterfaceTable &);
			bool Compare(const Address *) const;
		};

		virtual bool CreateTable();

		RouteEntry *rtable_begin, *rtable_end;
		Address defAddr;
	};

	class VirtualRouteTable : public RouteTable {
		// override from class  RouteTable
		virtual bool CreateTable();
	};

	RouteTable *GetRouteTable(bool = false);

	class ProxyCriterion {
		typedef PIPSocket::Address Address;

	public:
		ProxyCriterion() : network(0) { /* initialize later */ }
		~ProxyCriterion() { ClearTable(); }

		bool IsInternal(const Address & ip) const;
		bool Required(const Address &, const Address &) const;

		void LoadConfig(PConfig *);
		void ClearTable();

	private:
		int size;
		Address *network, *netmask;
	};

	bool ProxyRequired(const PIPSocket::Address & ip1, const PIPSocket::Address & ip2) const
	{ return m_ProxyCriterion.Required(ip1, ip2); }

	// Since PStringToString is not thread-safe,
	// I write this small class to replace that
	class RewriteData {
	public:
		RewriteData(PConfig *, const PString &);
		~RewriteData();
		PINDEX Size() const { return m_size; }
		const PString & Key(PINDEX i) const { return m_RewriteKey[i]; }
		const PString & Value(PINDEX i) const { return m_RewriteValue[i]; }
		const PStringArray & Values(PINDEX i) const { return m_RewriteValues[i]; }

	private:
		PString *m_RewriteKey, *m_RewriteValue;
		PStringArray *m_RewriteValues;
		PINDEX m_size;
	};

	class RewriteTool {
	public:
		RewriteTool() : m_Rewrite(0) {}
		~RewriteTool() { delete m_Rewrite; }
		void LoadConfig(PConfig *);
		bool RewritePString(PString &) const;

	private:
		PString m_RewriteFastmatch;
		char m_TrailingChar;
		RewriteData *m_Rewrite;
	};

	/// maybe modifies #alias#. returns true if it did
	bool RewriteE164(H225_AliasAddress & alias);
	bool RewriteE164(H225_ArrayOf_AliasAddress & aliases);

	bool RewritePString(PString & s) { return m_Rewrite.RewritePString(s); }

	PString GetGKHome(std::vector<PIPSocket::Address> &) const;
	void SetGKHome(const PStringArray &);

	// accessors
	/** Accessor and 'Factory' to the static Toolkit. 
	 * If you want to use your own Toolkit class you have to
	 * overwrite this method and ensure that your version is 
	 * called first -- before any other call to #Toolkit::Instance#.
	 * Example: 
	 * <pre>
	 * class MyToolkit: public Toolkit {
	 *  public: 
	 *   static Toolkit& Instance() {
	 *	   if (m_Instance == NULL) m_Instance = new MyToolkit();
	 *     return m_Instance;
	 *   }
	 * };
	 * void main() {
	 *   MyToolkit::Instance();
	 * }
	 * </pre>
	 */

	/** Accessor and 'Factory' for the global (static) configuration. 
	 * With this we are able to implement out own Config-Loader 
	 * in the same way as #Instance()#. And we can use #Config()# 
	 * in the constructor of #Toolkit# (and its descentants).
	 */
	PConfig* Config(); 
	PConfig* Config(const char *); 

	/** Sets the config that the toolkit uses to a given config.
	 *  A prior loaded Config is discarded. 
	 */
	PConfig* SetConfig(const PFilePath &fp, const PString &section);

	/* This method modifies the config from status port
	 * Warning: don't modify the config via status port and change config file simultaneously,
	 * or the config file may be messed up.
	 */
	void SetConfig(int act, const PString & sec, const PString & key = PString::Empty(), const PString & value = PString::Empty());

	PConfig* ReloadConfig();

	/// reads name of the running instance from config
	static const PString & GKName();

	/// returns an identification of the binary
	static const PString GKVersion();

	/** simplify PString regex matching.
	 * @param str String that should match the regex
	 * @param regexStr the string which is compiled to a regex and executed with #regex.Execute(str, pos)#
	 * @return TRUE if the regex matched and FALSE if not or any error case.
	 */
	static BOOL MatchRegex(const PString &str, const PString &regexStr);

	/** returns the #BOOL# that #str# represents. 
	 * Case insensitive, "t...", "y...", "a...", "1" are #TRUE#, all other values are #FALSE#.
	 */
	static bool AsBool(const PString & str);

	static void GetNetworkFromString(const PString &, PIPSocket::Address &, PIPSocket::Address &);

	static PString CypherDecode(const PString &, const PString &, int);

	/** you may add more extension codes in descendant classes. This codes will not be transferred
	 * or something it will be the return code of some methods for handling switches easy. */
	enum {
		iecUnknown     = -1,  /// internal extension code for an unknown triple(cntry,ext,manuf)
		iecFailoverRAS  = 1,   /// i.e.c. for "This RQ is a failover RQ" and must not be answerd.
		iecUserbase    = 1000 /// first guaranteed unused 'iec' by GnuGK Toolkit.
	};
	/** t35 extension or definitions as field for H225_NonStandardIdentifier */
	enum {
		t35cOpenOrg = 255,       /// country code for the "Open Source Organisation" Country
		t35mOpenOrg = 4242,     /// manufacurers code for the "Open Source Organisation"
		t35eFailoverRAS = 255  /// Defined HERE! 
	};
	/** If the triple #(country,extension,manufacturer)# represents an 
	 * extension known to the GnuGK this method returns its 'internal extension code' 
	 # #iecXXX' or #iecUnknow# otherwise.
	 *
	 * Overwriting methods should use a simlilar scheme and call
	 * <code>
	 * if(inherited::GnuGKExtension(country,extension,menufacturer) == iecUnknown) {
	 *   ...
	 *   (handle own cases)
	 *   ...
	 * }
	 * </code>
	 * This results in 'cascading' calls until a iec!=iecUnkown is returned.
	 */
	virtual int GetInternalExtensionCode(const unsigned &country, 
						 const unsigned &extension, 
						 const unsigned &manufacturer) const;
	
	int GetInternalExtensionCode(const H225_H221NonStandard& data) const;

	/** A c-string (#char*#) hash function that considers the
	 * whole string #name# ending with #\0#.
	 */
	inline static unsigned long HashCStr(const unsigned char *name) ;

	/** Generate a call id for accounting purposes, that is unique  
		during subsequent GK start/stop events.
		
		@return
		A string with the unique id.
	*/
	PString GenerateAcctSessionId();
	
 protected:
	void CreateConfig();

	PFilePath m_ConfigFilePath;
	PString   m_GKName;
	PString   m_ConfigDefaultSection;
	PConfig*  m_Config;
	bool	  m_ConfigDirty;

	RewriteTool m_Rewrite;
	BOOL      m_EmergencyAccept;

	RouteTable m_RouteTable;
	VirtualRouteTable m_VirtualRouteTable;
	ProxyCriterion m_ProxyCriterion;

	std::vector<PIPSocket::Address> m_GKHome;
	bool m_BindAll;

	/// a counter incremented for each generated session id
	long m_acctSessionCounter;
	/// base part for session id, changed with every GK restart
	long m_acctSessionBase;
	/// mutex for atomic session id generation (prevents from duplicates)
	PMutex m_acctSessionMutex;
	
 private:
	PFilePath m_tmpconfig;
};


inline unsigned long
Toolkit::HashCStr(const unsigned char *name) 
{
	register unsigned long h = 0, g;
	while (*name) {
		h = (h << 4) + *name++;
		if ( (g = (h & 0xf0000000)) ) h ^= g >> 24;
		h &= ~g;
	}
	return h;
}

inline PConfig *GkConfig()
{
	return Toolkit::Instance()->Config();
}

inline PConfig *GkConfig(const char *section)
{
	return Toolkit::Instance()->Config(section);
}

inline const PString & Toolkit::GKName()
{
	return Toolkit::Instance()->m_GKName;
}

#endif // TOOLKIT_H
