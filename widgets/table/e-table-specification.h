/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
#ifndef _E_TABLE_SPECIFICATION_H_
#define _E_TABLE_SPECIFICATION_H_

#include <gtk/gtkobject.h>
#include <gnome-xml/tree.h>
#include <gal/e-table/e-table-state.h>
#include <gal/e-table/e-table-column-specification.h>
#include <gal/e-table/e-table-defines.h>

#define E_TABLE_SPECIFICATION_TYPE        (e_table_specification_get_type ())
#define E_TABLE_SPECIFICATION(o)          (GTK_CHECK_CAST ((o), E_TABLE_SPECIFICATION_TYPE, ETableSpecification))
#define E_TABLE_SPECIFICATION_CLASS(k)    (GTK_CHECK_CLASS_CAST((k), E_TABLE_SPECIFICATION_TYPE, ETableSpecificationClass))
#define E_IS_TABLE_SPECIFICATION(o)       (GTK_CHECK_TYPE ((o), E_TABLE_SPECIFICATION_TYPE))
#define E_IS_TABLE_SPECIFICATION_CLASS(k) (GTK_CHECK_CLASS_TYPE ((k), E_TABLE_SPECIFICATION_TYPE))

typedef struct {
	GtkObject base;

	ETableColumnSpecification **columns;
	ETableState *state;

	guint no_headers : 1;
	guint click_to_add : 1;
	guint draw_grid : 1;
	ETableCursorMode cursor_mode;
	char *click_to_add_message_;
} ETableSpecification;

typedef struct {
	GtkObjectClass parent_class;
} ETableSpecificationClass;

GtkType              e_table_specification_get_type          (void);
ETableSpecification *e_table_specification_new               (void);

gboolean             e_table_specification_load_from_file    (ETableSpecification *specification,
							      const char          *filename);
void                 e_table_specification_load_from_string  (ETableSpecification *specification,
							      const char          *xml);
void                 e_table_specification_load_from_node    (ETableSpecification *specification,
							      const xmlNode       *node);

#if 0
void                 e_table_specification_save_to_file      (ETableSpecification *specification,
							      const char          *filename);
char                *e_table_specification_save_to_string    (ETableSpecification *specification);
xmlNode             *e_table_specification_save_to_node      (ETableSpecification *specification,
							      xmlDoc              *doc);
#endif

#endif /* _E_TABLE_SPECIFICATION_H_ */
