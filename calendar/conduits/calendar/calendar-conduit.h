/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* $Id$ */

#ifndef __CALENDAR_CONDUIT_H__
#define __CALENDAR_CONDUIT_H__

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <gnome.h>
#include <pi-datebook.h>
#include <gpilotd/gnome-pilot-conduit.h>
#include <gpilotd/gnome-pilot-conduit-standard-abs.h>
#include <cal-client/cal-client.h>
#include <cal-util/calobj.h>
#include <cal-util/timeutil.h>
#include <liboaf/liboaf.h>


/* This is the local record structure for the GnomeCal conduit. */
typedef struct _GCalLocalRecord GCalLocalRecord;
struct _GCalLocalRecord {
	/* The stuff from gnome-pilot-conduit-standard-abs.h
	   Must be first in the structure, or instances of this
	   structure cannot be used by gnome-pilot-conduit-standard-abs.
	*/
	LocalRecord local;
	/* The corresponding iCal object, as found by GnomeCal. */
	CalComponent *ical;
        /* pilot-link appointment structure, used for implementing Transmit. */	
	struct Appointment *a;
};
#define GCAL_LOCALRECORD(s) ((GCalLocalRecord*)(s))

/* This is the configuration of the GnomeCal conduit. */
typedef struct _GCalConduitCfg GCalConduitCfg;
struct _GCalConduitCfg {
	gboolean open_secret;
	guint32 pilotId;
	GnomePilotConduitSyncType  sync_type;   /* only used by capplet */
};
#define GET_GCALCONFIG(c) ((GCalConduitCfg*)gtk_object_get_data(GTK_OBJECT(c),"gcalconduit_cfg"))

/* This is the context for all the GnomeCal conduit methods. */
typedef struct _GCalConduitContext GCalConduitContext;
struct _GCalConduitContext {
	struct AppointmentAppInfo ai;
	GCalConduitCfg *cfg;
	CalClient *client;
	CORBA_Environment ev;
	CORBA_ORB orb;
	gboolean calendar_load_tried;
	gboolean calendar_load_success;

	char *calendar_file;
};
#define GET_GCALCONTEXT(c) ((GCalConduitContext*)gtk_object_get_data(GTK_OBJECT(c),"gcalconduit_context"))


/* Given a GCalConduitCfg*, allocates the structure and 
   loads the configuration data for the given pilot.
   this is defined in the header file because it is used by
   both calendar-conduit and calendar-conduit-control-applet,
   and we don't want to export any symbols we don't have to. */
static void 
gcalconduit_load_configuration(GCalConduitCfg **c,
			       guint32 pilotId) 
{
	gchar prefix[256];
	g_snprintf(prefix,255,"/gnome-pilot.d/calendard-conduit/Pilot_%u/",pilotId);
	
	*c = g_new0(GCalConduitCfg,1);
	g_assert(*c != NULL);
	gnome_config_push_prefix(prefix);
	(*c)->open_secret = gnome_config_get_bool("open_secret=FALSE");
	(*c)->sync_type = GnomePilotConduitSyncTypeCustom; /* set in capplets main */
	gnome_config_pop_prefix();
	
	(*c)->pilotId = pilotId;
}


#endif __CALENDAR_CONDUIT_H__ 
