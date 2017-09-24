/* GTK - The GIMP Toolkit
 *
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GSK_SL_NODE_PRIVATE_H__
#define __GSK_SL_NODE_PRIVATE_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _GskSlNode GskSlNode;
typedef struct _GskSlNodeClass GskSlNodeClass;

struct _GskSlNode {
  const GskSlNodeClass *class;
  guint ref_count;
};

struct _GskSlNodeClass {
  void                  (* free)                                (GskSlNode           *node);

  void                  (* print)                               (GskSlNode           *node,
                                                                 GString             *string);
};

GskSlNode *             gsk_sl_node_new_program                 (GBytes              *source,
                                                                 GError             **error);

GskSlNode *             gsk_sl_node_ref                         (GskSlNode           *node);
void                    gsk_sl_node_unref                       (GskSlNode           *node);

void                    gsk_sl_node_print                       (GskSlNode           *node,
                                                                 GString             *string);

G_END_DECLS

#endif /* __GSK_SL_NODE_PRIVATE_H__ */
