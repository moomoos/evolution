/*
 * e-mail-account-store.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#include "e-mail-account-store.h"

#include <config.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include <e-util/e-marshal.h>
#include <libevolution-utils/e-alert-dialog.h>

#include <libemail-engine/mail-ops.h>

#include <mail/mail-vfolder-ui.h>

#define E_MAIL_ACCOUNT_STORE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_ACCOUNT_STORE, EMailAccountStorePrivate))

typedef struct _IndexItem IndexItem;

struct _EMailAccountStorePrivate {
	CamelService *default_service;
	GHashTable *service_index;
	gchar *sort_order_filename;
	gboolean express_mode;
	gpointer session;  /* weak pointer */
	guint busy_count;
};

struct _IndexItem {
	CamelService *service;
	GtkTreeRowReference *reference;
	gulong notify_handler_id;
};

enum {
	PROP_0,
	PROP_BUSY,
	PROP_DEFAULT_SERVICE,
	PROP_EXPRESS_MODE,
	PROP_SESSION
};

enum {
	SERVICE_ADDED,
	SERVICE_REMOVED,
	SERVICE_ENABLED,
	SERVICE_DISABLED,
	SERVICES_REORDERED,
	REMOVE_REQUESTED,
	ENABLE_REQUESTED,
	DISABLE_REQUESTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* Forward Declarations */
static void	e_mail_account_store_interface_init
						(GtkTreeModelIface *interface);

G_DEFINE_TYPE_WITH_CODE (
	EMailAccountStore,
	e_mail_account_store,
	GTK_TYPE_LIST_STORE,
	G_IMPLEMENT_INTERFACE (
		GTK_TYPE_TREE_MODEL,
		e_mail_account_store_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

static void
index_item_free (IndexItem *item)
{
	g_signal_handler_disconnect (
		item->service, item->notify_handler_id);

	g_object_unref (item->service);
	gtk_tree_row_reference_free (item->reference);

	g_slice_free (IndexItem, item);
}

static gboolean
mail_account_store_get_iter (EMailAccountStore *store,
                             CamelService *service,
                             GtkTreeIter *iter)
{
	IndexItem *item;
	GtkTreeModel *model;
	GtkTreePath *path;
	gboolean iter_set;

	g_return_val_if_fail (service != NULL, FALSE);

	item = g_hash_table_lookup (store->priv->service_index, service);

	if (item == NULL)
		return FALSE;

	if (!gtk_tree_row_reference_valid (item->reference))
		return FALSE;

	model = gtk_tree_row_reference_get_model (item->reference);
	path = gtk_tree_row_reference_get_path (item->reference);
	iter_set = gtk_tree_model_get_iter (model, iter, path);
	gtk_tree_path_free (path);

	return iter_set;
}

static gint
mail_account_store_default_compare (CamelService *service_a,
                                    CamelService *service_b,
                                    EMailAccountStore *store)
{
	const gchar *display_name_a;
	const gchar *display_name_b;
	const gchar *uid_a;
	const gchar *uid_b;

	uid_a = camel_service_get_uid (service_a);
	uid_b = camel_service_get_uid (service_b);

	/* Check for special cases first. */

	if (e_mail_account_store_get_express_mode (store)) {
		if (g_str_equal (uid_a, E_MAIL_SESSION_LOCAL_UID) &&
		    g_str_equal (uid_b, E_MAIL_SESSION_VFOLDER_UID))
			return -1;
		else if (g_str_equal (uid_b, E_MAIL_SESSION_LOCAL_UID) &&
			 g_str_equal (uid_a, E_MAIL_SESSION_VFOLDER_UID))
			return 1;
		else if (g_str_equal (uid_a, E_MAIL_SESSION_LOCAL_UID))
			return 1;
		else if (g_str_equal (uid_b, E_MAIL_SESSION_LOCAL_UID))
			return -1;
		else if (g_str_equal (uid_a, E_MAIL_SESSION_VFOLDER_UID))
			return 1;
		else if (g_str_equal (uid_a, E_MAIL_SESSION_VFOLDER_UID))
			return -1;
	} else {
		if (g_str_equal (uid_a, E_MAIL_SESSION_LOCAL_UID))
			return -1;
		else if (g_str_equal (uid_b, E_MAIL_SESSION_LOCAL_UID))
			return 1;
		else if (g_str_equal (uid_a, E_MAIL_SESSION_VFOLDER_UID))
			return 1;
		else if (g_str_equal (uid_b, E_MAIL_SESSION_VFOLDER_UID))
			return -1;
	}

	/* Otherwise sort them alphabetically. */

	display_name_a = camel_service_get_display_name (service_a);
	display_name_b = camel_service_get_display_name (service_b);

	if (display_name_a == NULL)
		display_name_a = "";

	if (display_name_b == NULL)
		display_name_b = "";

	return g_utf8_collate (display_name_a, display_name_b);
}

static void
mail_account_store_update_row (EMailAccountStore *store,
                               CamelService *service,
                               GtkTreeIter *iter)
{
	CamelProvider *provider;
	gboolean is_default;
	const gchar *backend_name;
	const gchar *display_name;

	is_default = (service == store->priv->default_service);
	display_name = camel_service_get_display_name (service);

	provider = camel_service_get_provider (service);
	backend_name = (provider != NULL) ? provider->protocol : NULL;

	gtk_list_store_set (
		GTK_LIST_STORE (store), iter,
		E_MAIL_ACCOUNT_STORE_COLUMN_DEFAULT, is_default,
		E_MAIL_ACCOUNT_STORE_COLUMN_BACKEND_NAME, backend_name,
		E_MAIL_ACCOUNT_STORE_COLUMN_DISPLAY_NAME, display_name,
		-1);
}

static void
mail_account_store_service_notify_cb (CamelService *service,
                                      GParamSpec *pspec,
                                      EMailAccountStore *store)
{
	GtkTreeIter iter;

	if (mail_account_store_get_iter (store, service, &iter))
		mail_account_store_update_row (store, service, &iter);
}

static void
mail_account_store_remove_source_cb (ESource *source,
                                     GAsyncResult *result,
                                     EMailAccountStore *store)
{
	GError *error = NULL;

	/* FIXME EMailAccountStore should implement EAlertSink. */
	if (!e_source_remove_finish (source, result, &error)) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_return_if_fail (store->priv->busy_count > 0);
	store->priv->busy_count--;
	g_object_notify (G_OBJECT (store), "busy");

	g_object_unref (store);
}

static void
mail_account_store_write_source_cb (ESource *source,
                                    GAsyncResult *result,
                                    EMailAccountStore *store)
{
	GError *error = NULL;

	/* FIXME EMailAccountStore should implement EAlertSink. */
	if (!e_source_write_finish (source, result, &error)) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_return_if_fail (store->priv->busy_count > 0);
	store->priv->busy_count--;
	g_object_notify (G_OBJECT (store), "busy");

	g_object_unref (store);
}

static void
mail_account_store_clean_index (EMailAccountStore *store)
{
	GQueue trash = G_QUEUE_INIT;
	GHashTable *hash_table;
	GHashTableIter iter;
	gpointer key, value;

	hash_table = store->priv->service_index;
	g_hash_table_iter_init (&iter, hash_table);

	/* Remove index items with invalid GtkTreeRowReferences. */

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		IndexItem *item = value;

		if (!gtk_tree_row_reference_valid (item->reference))
			g_queue_push_tail (&trash, key);
	}

	while ((key = g_queue_pop_head (&trash)) != NULL)
		g_hash_table_remove (hash_table, key);
}

static void
mail_account_store_update_index (EMailAccountStore *store,
                                 GtkTreePath *path,
                                 GtkTreeIter *iter)
{
	CamelService *service = NULL;
	GHashTable *hash_table;
	GtkTreeModel *model;
	IndexItem *item;

	model = GTK_TREE_MODEL (store);
	hash_table = store->priv->service_index;

	gtk_tree_model_get (
		model, iter,
		E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE, &service, -1);

	if (service == NULL)
		return;

	item = g_hash_table_lookup (hash_table, service);

	if (item == NULL) {
		item = g_slice_new0 (IndexItem);
		item->service = g_object_ref (service);

		item->notify_handler_id = g_signal_connect (
			service, "notify", G_CALLBACK (
			mail_account_store_service_notify_cb), store);

		g_hash_table_insert (hash_table, item->service, item);
	}

	/* Update the row reference so the IndexItem will survive
	 * drag-and-drop (new row is inserted, old row is deleted). */
	gtk_tree_row_reference_free (item->reference);
	item->reference = gtk_tree_row_reference_new (model, path);

	g_object_unref (service);
}

static void
mail_account_store_set_session (EMailAccountStore *store,
                                EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (store->priv->session == NULL);

	store->priv->session = session;

	g_object_add_weak_pointer (
		G_OBJECT (store->priv->session),
		&store->priv->session);
}

static void
mail_account_store_set_property (GObject *object,
                                 guint property_id,
                                 const GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DEFAULT_SERVICE:
			e_mail_account_store_set_default_service (
				E_MAIL_ACCOUNT_STORE (object),
				g_value_get_object (value));
			return;

		case PROP_EXPRESS_MODE:
			e_mail_account_store_set_express_mode (
				E_MAIL_ACCOUNT_STORE (object),
				g_value_get_boolean (value));
			return;

		case PROP_SESSION:
			mail_account_store_set_session (
				E_MAIL_ACCOUNT_STORE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_account_store_get_property (GObject *object,
                                 guint property_id,
                                 GValue *value,
                                 GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BUSY:
			g_value_set_boolean (
				value,
				e_mail_account_store_get_busy (
				E_MAIL_ACCOUNT_STORE (object)));
			return;

		case PROP_DEFAULT_SERVICE:
			g_value_set_object (
				value,
				e_mail_account_store_get_default_service (
				E_MAIL_ACCOUNT_STORE (object)));
			return;

		case PROP_EXPRESS_MODE:
			g_value_set_boolean (
				value,
				e_mail_account_store_get_express_mode (
				E_MAIL_ACCOUNT_STORE (object)));
			return;

		case PROP_SESSION:
			g_value_set_object (
				value,
				e_mail_account_store_get_session (
				E_MAIL_ACCOUNT_STORE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_account_store_dispose (GObject *object)
{
	EMailAccountStorePrivate *priv;

	priv = E_MAIL_ACCOUNT_STORE_GET_PRIVATE (object);

	if (priv->session != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->session), &priv->session);
		priv->session = NULL;
	}

	if (priv->default_service != NULL) {
		g_object_unref (priv->default_service);
		priv->default_service = NULL;
	}

	g_hash_table_remove_all (priv->service_index);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_account_store_parent_class)->dispose (object);
}

static void
mail_account_store_finalize (GObject *object)
{
	EMailAccountStorePrivate *priv;

	priv = E_MAIL_ACCOUNT_STORE_GET_PRIVATE (object);

	g_warn_if_fail (priv->busy_count == 0);
	g_hash_table_destroy (priv->service_index);
	g_free (priv->sort_order_filename);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_account_store_parent_class)->finalize (object);
}

static void
mail_account_store_constructed (GObject *object)
{
	EMailAccountStore *store;
	EMailSession *session;
	ESourceRegistry *registry;
	const gchar *config_dir;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_account_store_parent_class)->constructed (object);

	store = E_MAIL_ACCOUNT_STORE (object);
	session = e_mail_account_store_get_session (store);
	registry = e_mail_session_get_registry (session);

	/* Bind the default mail account ESource to our default
	 * CamelService, with help from some transform functions. */
	g_object_bind_property_full (
		registry, "default-mail-account",
		store, "default-service",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_source_to_service,
		e_binding_transform_service_to_source,
		session, (GDestroyNotify) NULL);

	config_dir = mail_session_get_config_dir ();

	/* XXX Should we take the filename as a constructor property? */
	store->priv->sort_order_filename = g_build_filename (
		config_dir, "sortorder.ini", NULL);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
mail_account_store_service_added (EMailAccountStore *store,
                                  CamelService *service)
{
	/* Placeholder so subclasses can safely chain up. */
}

static void
mail_account_store_service_removed (EMailAccountStore *store,
                                    CamelService *service)
{
	EMailSession *session;
	ESourceRegistry *registry;
	ESource *source;
	const gchar *uid;

	session = e_mail_account_store_get_session (store);
	registry = e_mail_session_get_registry (session);

	uid = camel_service_get_uid (service);
	source = e_source_registry_ref_source (registry, uid);

	/* If this ESource is part of a collection, we need to remove
	 * the entire collection.  Check the ESource and its ancestors
	 * for a collection extension and remove the containing source. */
	if (source != NULL) {
		ESource *collection;

		collection = e_source_registry_find_extension (
			registry, source, E_SOURCE_EXTENSION_COLLECTION);
		if (collection != NULL) {
			g_object_unref (source);
			source = collection;
		}
	}

	if (source != NULL) {
		store->priv->busy_count++;
		g_object_notify (G_OBJECT (store), "busy");

		/* XXX Should this be cancellable? */
		e_source_remove (
			source, NULL, (GAsyncReadyCallback)
			mail_account_store_remove_source_cb,
			g_object_ref (store));

		g_object_unref (source);
	}
}

static void
mail_account_store_service_enabled (EMailAccountStore *store,
                                    CamelService *service)
{
	EMailSession *session;
	ESourceRegistry *registry;
	ESource *source;
	const gchar *uid;

	session = e_mail_account_store_get_session (store);
	registry = e_mail_session_get_registry (session);

	uid = camel_service_get_uid (service);
	source = e_source_registry_ref_source (registry, uid);

	if (source != NULL) {
		e_source_set_enabled (source, TRUE);

		store->priv->busy_count++;
		g_object_notify (G_OBJECT (store), "busy");

		/* XXX Should this be cancellable? */
		e_source_write (
			source, NULL, (GAsyncReadyCallback)
			mail_account_store_write_source_cb,
			g_object_ref (store));

		g_object_unref (source);
	}
}

static void
mail_account_store_service_disabled (EMailAccountStore *store,
                                     CamelService *service)
{
	EMailSession *session;
	ESourceRegistry *registry;
	ESource *source;
	const gchar *uid;

	session = e_mail_account_store_get_session (store);
	registry = e_mail_session_get_registry (session);

	uid = camel_service_get_uid (service);
	source = e_source_registry_ref_source (registry, uid);

	if (source != NULL) {
		e_source_set_enabled (source, FALSE);

		store->priv->busy_count++;
		g_object_notify (G_OBJECT (store), "busy");

		/* XXX Should this be cancellable? */
		e_source_write (
			source, NULL, (GAsyncReadyCallback)
			mail_account_store_write_source_cb,
			g_object_ref (store));

		g_object_unref (source);
	}
}

static void
mail_account_store_services_reordered (EMailAccountStore *store,
                                       gboolean default_restored)
{
	/* XXX Should this be made asynchronous? */

	GError *error = NULL;

	if (default_restored) {
		const gchar *filename;

		filename = store->priv->sort_order_filename;

		if (g_file_test (filename, G_FILE_TEST_EXISTS))
			g_unlink (filename);

		return;
	}

	if (!e_mail_account_store_save_sort_order (store, &error)) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
}

static gboolean
mail_account_store_remove_requested (EMailAccountStore *store,
                                     GtkWindow *parent_window,
                                     CamelService *service)
{
	gint response;

	/* FIXME Need to use "mail:ask-delete-account-with-proxies" if the
	 *       mail account has proxies.  But this is groupwise-specific
	 *       and doesn't belong here anyway.  Think of a better idea. */

	response = e_alert_run_dialog_for_args (
		parent_window, "mail:ask-delete-account", NULL);

	return (response == GTK_RESPONSE_YES);
}

static gboolean
mail_account_store_enable_requested (EMailAccountStore *store,
                                     GtkWindow *parent_window,
                                     CamelService *service)
{
	return TRUE;
}

static gboolean
mail_account_store_disable_requested (EMailAccountStore *store,
                                      GtkWindow *parent_window,
                                      CamelService *service)
{
	/* FIXME Need to check whether the account has proxies and run a
	 *       "mail:ask-delete-proxy-accounts" alert dialog, but this
	 *       is groupwise-specific and doesn't belong here anyway.
	 *       Think of a better idea. */

	return TRUE;
}

static void
mail_account_store_row_changed (GtkTreeModel *tree_model,
                                GtkTreePath *path,
                                GtkTreeIter *iter)
{
	EMailAccountStore *store;

	/* Neither GtkTreeModel nor GtkListStore implements
	 * this method, so there is nothing to chain up to. */

	store = E_MAIL_ACCOUNT_STORE (tree_model);
	mail_account_store_update_index (store, path, iter);
}

static void
mail_account_store_row_inserted (GtkTreeModel *tree_model,
                                 GtkTreePath *path,
                                 GtkTreeIter *iter)
{
	EMailAccountStore *store;

	/* Neither GtkTreeModel nor GtkListStore implements
	 * this method, so there is nothing to chain up to. */

	store = E_MAIL_ACCOUNT_STORE (tree_model);
	mail_account_store_update_index (store, path, iter);
}

static gboolean
mail_account_store_true_proceed (GSignalInvocationHint *ihint,
                                 GValue *return_accumulator,
                                 const GValue *handler_return,
                                 gpointer not_used)
{
	gboolean proceed;

	proceed = g_value_get_boolean (handler_return);
	g_value_set_boolean (return_accumulator, proceed);

	return proceed;
}

static void
e_mail_account_store_class_init (EMailAccountStoreClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EMailAccountStorePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_account_store_set_property;
	object_class->get_property = mail_account_store_get_property;
	object_class->dispose = mail_account_store_dispose;
	object_class->finalize = mail_account_store_finalize;
	object_class->constructed = mail_account_store_constructed;

	class->service_added = mail_account_store_service_added;
	class->service_removed = mail_account_store_service_removed;
	class->service_enabled = mail_account_store_service_enabled;
	class->service_disabled = mail_account_store_service_disabled;
	class->services_reordered = mail_account_store_services_reordered;
	class->remove_requested = mail_account_store_remove_requested;
	class->enable_requested = mail_account_store_enable_requested;
	class->disable_requested = mail_account_store_disable_requested;

	g_object_class_install_property (
		object_class,
		PROP_BUSY,
		g_param_spec_boolean (
			"busy",
			"Busy",
			"Whether async operations are in progress",
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_DEFAULT_SERVICE,
		g_param_spec_object (
			"default-service",
			"Default Service",
			"Default mail store",
			CAMEL_TYPE_SERVICE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_EXPRESS_MODE,
		g_param_spec_boolean (
			"express-mode",
			"Express Mode",
			"Whether express mode is enabled",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			"Session",
			"Mail session",
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	signals[SERVICE_ADDED] = g_signal_new (
		"service-added",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, service_added),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		CAMEL_TYPE_SERVICE);

	signals[SERVICE_REMOVED] = g_signal_new (
		"service-removed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, service_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		CAMEL_TYPE_SERVICE);

	signals[SERVICE_ENABLED] = g_signal_new (
		"service-enabled",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, service_enabled),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		CAMEL_TYPE_SERVICE);

	signals[SERVICE_DISABLED] = g_signal_new (
		"service-disabled",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, service_disabled),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		CAMEL_TYPE_SERVICE);

	signals[SERVICES_REORDERED] = g_signal_new (
		"services-reordered",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, services_reordered),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOOLEAN,
		G_TYPE_NONE, 1,
		G_TYPE_BOOLEAN);

	signals[REMOVE_REQUESTED] = g_signal_new (
		"remove-requested",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, remove_requested),
		mail_account_store_true_proceed, NULL,
		e_marshal_BOOLEAN__OBJECT_OBJECT,
		G_TYPE_BOOLEAN, 2,
		GTK_TYPE_WINDOW,
		CAMEL_TYPE_SERVICE);

	signals[ENABLE_REQUESTED] = g_signal_new (
		"enable-requested",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, enable_requested),
		mail_account_store_true_proceed, NULL,
		e_marshal_BOOLEAN__OBJECT_OBJECT,
		G_TYPE_BOOLEAN, 2,
		GTK_TYPE_WINDOW,
		CAMEL_TYPE_SERVICE);

	signals[DISABLE_REQUESTED] = g_signal_new (
		"disable-requested",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMailAccountStoreClass, disable_requested),
		mail_account_store_true_proceed, NULL,
		e_marshal_BOOLEAN__OBJECT_OBJECT,
		G_TYPE_BOOLEAN, 2,
		GTK_TYPE_WINDOW,
		CAMEL_TYPE_SERVICE);
}

static void
e_mail_account_store_interface_init (GtkTreeModelIface *interface)
{
	interface->row_changed = mail_account_store_row_changed;
	interface->row_inserted = mail_account_store_row_inserted;
}

static void
e_mail_account_store_init (EMailAccountStore *store)
{
	GType types[E_MAIL_ACCOUNT_STORE_NUM_COLUMNS];
	GHashTable *service_index;
	gint ii = 0;

	service_index = g_hash_table_new_full (
		(GHashFunc) g_direct_hash,
		(GEqualFunc) g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) index_item_free);

	store->priv = E_MAIL_ACCOUNT_STORE_GET_PRIVATE (store);
	store->priv->service_index = service_index;

	types[ii++] = CAMEL_TYPE_SERVICE;	/* COLUMN_SERVICE */
	types[ii++] = G_TYPE_BOOLEAN;		/* COLUMN_BUILTIN */
	types[ii++] = G_TYPE_BOOLEAN;		/* COLUMN_ENABLED */
	types[ii++] = G_TYPE_BOOLEAN;		/* COLUMN_DEFAULT */
	types[ii++] = G_TYPE_STRING;		/* COLUMN_BACKEND_NAME */
	types[ii++] = G_TYPE_STRING;		/* COLUMN_DISPLAY_NAME */

	g_assert (ii == E_MAIL_ACCOUNT_STORE_NUM_COLUMNS);

	gtk_list_store_set_column_types (
		GTK_LIST_STORE (store),
		G_N_ELEMENTS (types), types);
}

EMailAccountStore *
e_mail_account_store_new (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		E_TYPE_MAIL_ACCOUNT_STORE,
		"session", session, NULL);
}

void
e_mail_account_store_clear (EMailAccountStore *store)
{
	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));

	gtk_list_store_clear (GTK_LIST_STORE (store));
	g_hash_table_remove_all (store->priv->service_index);
}

gboolean
e_mail_account_store_get_busy (EMailAccountStore *store)
{
	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), FALSE);

	return (store->priv->busy_count > 0);
}

EMailSession *
e_mail_account_store_get_session (EMailAccountStore *store)
{
	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), NULL);

	return E_MAIL_SESSION (store->priv->session);
}

CamelService *
e_mail_account_store_get_default_service (EMailAccountStore *store)
{
	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), NULL);

	return store->priv->default_service;
}

void
e_mail_account_store_set_default_service (EMailAccountStore *store,
                                          CamelService *service)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean iter_set;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));

	if (service == store->priv->default_service)
		return;

	if (service != NULL) {
		g_return_if_fail (CAMEL_IS_SERVICE (service));
		g_object_ref (service);
	}

	if (store->priv->default_service != NULL)
		g_object_unref (store->priv->default_service);

	store->priv->default_service = service;

	model = GTK_TREE_MODEL (store);
	iter_set = gtk_tree_model_get_iter_first (model, &iter);

	while (iter_set) {
		CamelService *candidate;

		gtk_tree_model_get (
			model, &iter,
			E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE,
			&candidate, -1);

		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
			E_MAIL_ACCOUNT_STORE_COLUMN_DEFAULT,
			service == candidate, -1);

		g_object_unref (candidate);

		iter_set = gtk_tree_model_iter_next (model, &iter);
	}

	g_object_notify (G_OBJECT (store), "default-service");
}

gboolean
e_mail_account_store_get_express_mode (EMailAccountStore *store)
{
	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), FALSE);

	return store->priv->express_mode;
}

void
e_mail_account_store_set_express_mode (EMailAccountStore *store,
                                       gboolean express_mode)
{
	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));

	store->priv->express_mode = express_mode;

	g_object_notify (G_OBJECT (store), "express-mode");
}

void
e_mail_account_store_add_service (EMailAccountStore *store,
                                  CamelService *service)
{
	GSettings *settings;
	GtkTreeIter iter;
	const gchar *filename;
	const gchar *uid;
	gboolean builtin;
	gboolean enabled;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	/* Avoid duplicate services in the account store. */
	if (mail_account_store_get_iter (store, service, &iter))
		g_return_if_reached ();

	uid = camel_service_get_uid (service);

	/* Handle built-in services that don't have an EAccount. */

	if (g_strcmp0 (uid, E_MAIL_SESSION_LOCAL_UID) == 0) {
		builtin = TRUE;

		settings = g_settings_new ("org.gnome.evolution.mail");
		enabled = g_settings_get_boolean (settings, "enable-local");
		g_object_unref (settings);

	} else if (g_strcmp0 (uid, E_MAIL_SESSION_VFOLDER_UID) == 0) {
		builtin = TRUE;

		settings = g_settings_new ("org.gnome.evolution.mail");
		enabled = g_settings_get_boolean (settings, "enable-vfolders");
		g_object_unref (settings);

	} else {
		EMailSession *session;
		ESourceRegistry *registry;
		ESource *source;

		session = e_mail_account_store_get_session (store);

		registry = e_mail_session_get_registry (session);
		source = e_source_registry_ref_source (registry, uid);
		g_return_if_fail (source != NULL);

		builtin = FALSE;
		enabled = e_source_get_enabled (source);

		g_object_unref (source);
	}

	/* Where do we insert new services now that accounts can be
	 * reordered?  This is just a simple policy I came up with.
	 * It's certainly subject to debate and tweaking.
	 *
	 * Always insert new services in row 0 initially.  Then test
	 * for the presence of the sort order file.  If present, the
	 * user has messed around with the ordering so leave the new
	 * service at row 0.  If not present, services are sorted in
	 * their default order.  So re-apply the default order using
	 * e_mail_account_store_reorder_services(store, NULL) so the
	 * new service moves to its proper default position. */

	gtk_list_store_prepend (GTK_LIST_STORE (store), &iter);

	gtk_list_store_set (
		GTK_LIST_STORE (store), &iter,
		E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE, service,
		E_MAIL_ACCOUNT_STORE_COLUMN_BUILTIN, builtin,
		E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED, enabled,
		-1);

	/* This populates the rest of the columns. */
	mail_account_store_update_row (store, service, &iter);

	/* No need to connect to "service-added" emissions since it's
	 * always immediately followed by either "service-enabled" or
	 * "service-disabled" in MailFolderCache */

	g_signal_emit (store, signals[SERVICE_ADDED], 0, service);

	if (enabled)
		g_signal_emit (store, signals[SERVICE_ENABLED], 0, service);
	else
		g_signal_emit (store, signals[SERVICE_DISABLED], 0, service);

	filename = store->priv->sort_order_filename;

	if (!g_file_test (filename, G_FILE_TEST_EXISTS))
		e_mail_account_store_reorder_services (store, NULL);
}

void
e_mail_account_store_remove_service (EMailAccountStore *store,
                                     GtkWindow *parent_window,
                                     CamelService *service)
{
	GtkTreeIter iter;
	gboolean proceed;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	if (!mail_account_store_get_iter (store, service, &iter))
		g_return_if_reached ();

	/* If no parent window was given, skip the request signal. */
	if (GTK_IS_WINDOW (parent_window))
		g_signal_emit (
			store, signals[REMOVE_REQUESTED], 0,
			parent_window, service, &proceed);

	if (proceed) {
		g_object_ref (service);

		gtk_list_store_remove (GTK_LIST_STORE (store), &iter);

		mail_account_store_clean_index (store);

		g_signal_emit (store, signals[SERVICE_REMOVED], 0, service);

		g_object_unref (service);
	}
}

void
e_mail_account_store_enable_service (EMailAccountStore *store,
                                     GtkWindow *parent_window,
                                     CamelService *service)
{
	GtkTreeIter iter;
	gboolean proceed;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	if (!mail_account_store_get_iter (store, service, &iter))
		g_return_if_reached ();

	/* If no parent window was given, skip the request signal. */
	if (GTK_IS_WINDOW (parent_window))
		g_signal_emit (
			store, signals[ENABLE_REQUESTED], 0,
			parent_window, service, &proceed);

	if (proceed) {
		gtk_list_store_set (
			GTK_LIST_STORE (store), &iter,
			E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED, TRUE, -1);
		g_signal_emit (store, signals[SERVICE_ENABLED], 0, service);
	}
}

void
e_mail_account_store_disable_service (EMailAccountStore *store,
                                      GtkWindow *parent_window,
                                      CamelService *service)
{
	GtkTreeIter iter;
	gboolean proceed;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	if (!mail_account_store_get_iter (store, service, &iter))
		g_return_if_reached ();

	/* If no parent window was given, skip the request signal. */
	if (GTK_IS_WINDOW (parent_window))
		g_signal_emit (
			store, signals[DISABLE_REQUESTED], 0,
			parent_window, service, &proceed);

	if (proceed) {
		gtk_list_store_set (
			GTK_LIST_STORE (store), &iter,
			E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED, FALSE, -1);
		g_signal_emit (store, signals[SERVICE_DISABLED], 0, service);
	}
}

void
e_mail_account_store_queue_services (EMailAccountStore *store,
                                     GQueue *out_queue)
{
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean iter_set;
	gint column;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));
	g_return_if_fail (out_queue != NULL);

	tree_model = GTK_TREE_MODEL (store);

	iter_set = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (iter_set) {
		GValue value = G_VALUE_INIT;

		column = E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE;
		gtk_tree_model_get_value (tree_model, &iter, column, &value);
		g_queue_push_tail (out_queue, g_value_get_object (&value));
		g_value_unset (&value);

		iter_set = gtk_tree_model_iter_next (tree_model, &iter);
	}
}

void
e_mail_account_store_queue_enabled_services (EMailAccountStore *store,
                                             GQueue *out_queue)
{
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	gboolean iter_set;
	gint column;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));
	g_return_if_fail (out_queue != NULL);

	tree_model = GTK_TREE_MODEL (store);

	iter_set = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (iter_set) {
		GValue value = G_VALUE_INIT;
		gboolean enabled;

		column = E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED;
		gtk_tree_model_get_value (tree_model, &iter, column, &value);
		enabled = g_value_get_boolean (&value);
		g_value_unset (&value);

		if (enabled) {
			column = E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE;
			gtk_tree_model_get_value (tree_model, &iter, column, &value);
			g_queue_push_tail (out_queue, g_value_get_object (&value));
			g_value_unset (&value);
		}

		iter_set = gtk_tree_model_iter_next (tree_model, &iter);
	}
}

void
e_mail_account_store_reorder_services (EMailAccountStore *store,
                                       GQueue *ordered_services)
{
	GQueue *current_order = NULL;
	GQueue *default_order = NULL;
	GtkTreeModel *tree_model;
	gboolean use_default_order;
	GList *head, *link;
	gint *new_order;
	gint n_children;
	gint new_pos = 0;

	g_return_if_fail (E_IS_MAIL_ACCOUNT_STORE (store));

	tree_model = GTK_TREE_MODEL (store);
	n_children = gtk_tree_model_iter_n_children (tree_model, NULL);

	/* Treat NULL queues and empty queues the same. */
	if (ordered_services != NULL && g_queue_is_empty (ordered_services))
		ordered_services = NULL;

	/* If the length of the custom ordering disagrees with the
	 * number of rows in the store, revert to default ordering. */
	if (ordered_services != NULL) {
		if (g_queue_get_length (ordered_services) != n_children)
			ordered_services = NULL;
	}

	use_default_order = (ordered_services == NULL);

	/* Build a queue of CamelServices in the order they appear in
	 * the list store.  We'll use this to construct the mapping to
	 * pass to gtk_list_store_reorder(). */
	current_order = g_queue_new ();
	e_mail_account_store_queue_services (store, current_order);

	/* If a custom ordering was not given, revert to default. */
	if (use_default_order) {
		default_order = g_queue_copy (current_order);

		g_queue_sort (
			default_order, (GCompareDataFunc)
			mail_account_store_default_compare, store);

		ordered_services = default_order;
	}

	new_order = g_new0 (gint, n_children);
	head = g_queue_peek_head_link (ordered_services);

	for (link = head; link != NULL; link = g_list_next (link)) {
		GList *matching_link;
		gint old_pos;

		matching_link = g_queue_find (current_order, link->data);

		if (matching_link == NULL || matching_link->data == NULL)
			break;

		old_pos = g_queue_link_index (current_order, matching_link);

		matching_link->data = NULL;
		new_order[new_pos++] = old_pos;
	}

	if (new_pos == n_children) {
		gtk_list_store_reorder (GTK_LIST_STORE (store), new_order);
		g_signal_emit (
			store, signals[SERVICES_REORDERED], 0,
			use_default_order);
	}

	g_free (new_order);

	if (current_order != NULL)
		g_queue_free (current_order);

	if (default_order != NULL)
		g_queue_free (default_order);
}

gint
e_mail_account_store_compare_services (EMailAccountStore *store,
                                       CamelService *service_a,
                                       CamelService *service_b)
{
	GtkTreeModel *model;
	GtkTreePath *path_a;
	GtkTreePath *path_b;
	GtkTreeIter iter_a;
	GtkTreeIter iter_b;
	gboolean iter_a_set;
	gboolean iter_b_set;
	gint result;

	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), -1);
	g_return_val_if_fail (CAMEL_IS_SERVICE (service_a), -1);
	g_return_val_if_fail (CAMEL_IS_SERVICE (service_b), -1);

	/* XXX This is horribly inefficient but should be
	 *     over a small enough set to not be noticable. */

	iter_a_set = mail_account_store_get_iter (store, service_a, &iter_a);
	iter_b_set = mail_account_store_get_iter (store, service_b, &iter_b);

	if (!iter_a_set && !iter_b_set)
		return 0;

	if (!iter_a_set)
		return -1;

	if (!iter_b_set)
		return 1;

	model = GTK_TREE_MODEL (store);

	path_a = gtk_tree_model_get_path (model, &iter_a);
	path_b = gtk_tree_model_get_path (model, &iter_b);

	result = gtk_tree_path_compare (path_a, path_b);

	gtk_tree_path_free (path_a);
	gtk_tree_path_free (path_b);

	return result;
}

gboolean
e_mail_account_store_load_sort_order (EMailAccountStore *store,
                                      GError **error)
{
	GQueue service_queue = G_QUEUE_INIT;
	EMailSession *session;
	GKeyFile *key_file;
	const gchar *filename;
	gchar **service_uids;
	gboolean success = TRUE;
	gsize ii, length;

	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), FALSE);

	session = e_mail_account_store_get_session (store);

	key_file = g_key_file_new ();
	filename = store->priv->sort_order_filename;

	if (g_file_test (filename, G_FILE_TEST_EXISTS))
		success = g_key_file_load_from_file (
			key_file, filename, G_KEY_FILE_NONE, error);

	if (!success) {
		g_key_file_free (key_file);
		return FALSE;
	}

	/* If the key is not present, length is set to zero. */
	service_uids = g_key_file_get_string_list (
		key_file, "Accounts", "SortOrder", &length, NULL);

	for (ii = 0; ii < length; ii++) {
		CamelService *service;

		service = camel_session_get_service (
			CAMEL_SESSION (session), service_uids[ii]);
		if (service != NULL)
			g_queue_push_tail (&service_queue, service);
	}

	e_mail_account_store_reorder_services (store, &service_queue);

	g_queue_clear (&service_queue);
	g_strfreev (service_uids);

	g_key_file_free (key_file);

	return TRUE;
}

gboolean
e_mail_account_store_save_sort_order (EMailAccountStore *store,
                                      GError **error)
{
	GKeyFile *key_file;
	GtkTreeModel *model;
	GtkTreeIter iter;
	const gchar **service_uids;
	const gchar *filename;
	gchar *contents;
	gboolean iter_set;
	gboolean success;
	gsize length;
	gsize ii = 0;

	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (store), FALSE);

	model = GTK_TREE_MODEL (store);
	length = gtk_tree_model_iter_n_children (model, NULL);

	/* Empty store, nothing to save. */
	if (length == 0)
		return TRUE;

	service_uids = g_new0 (const gchar *, length);

	iter_set = gtk_tree_model_get_iter_first (model, &iter);

	while (iter_set) {
		GValue value = G_VALUE_INIT;
		const gint column = E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE;
		CamelService *service;

		gtk_tree_model_get_value (model, &iter, column, &value);
		service = g_value_get_object (&value);
		service_uids[ii++] = camel_service_get_uid (service);
		g_value_unset (&value);

		iter_set = gtk_tree_model_iter_next (model, &iter);
	}

	key_file = g_key_file_new ();
	filename = store->priv->sort_order_filename;

	g_key_file_set_string_list (
		key_file, "Accounts", "SortOrder", service_uids, length);

	contents = g_key_file_to_data (key_file, &length, NULL);
	success = g_file_set_contents (filename, contents, length, error);
	g_free (contents);

	g_key_file_free (key_file);

	g_free (service_uids);

	return success;
}

