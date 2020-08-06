
/** \file gauge.h
 *  \brief Header: WGauge widget
 */

#ifndef MC__WIDGET_GAUGE_H
#define MC__WIDGET_GAUGE_H

/*** typedefs(not structures) and defined constants **********************************************/

#define GAUGE(x) ((WGauge *)(x))

/*** enums ***************************************************************************************/

/*** structures declarations (and typedefs of structures)*****************************************/

typedef struct WGauge
{
    Widget widget;
    bool shown;
    int max;
    int current;
    bool from_left_to_right;
} WGauge;

/*** global variables defined in .c file *********************************************************/

/*** declarations of public functions ************************************************************/

WGauge *gauge_new (int y, int x, int cols, bool shown, int max, int current);
void gauge_set_value (WGauge * g, int max, int current);
void gauge_show (WGauge * g, bool shown);

/*** inline functions ****************************************************************************/

#endif /* MC__WIDGET_GAUGE_H */
