#include<stdlib.h>
#include<unistd.h>
#include<stdio.h>
#include<xcb/xcb.h>
#include<cairo-xcb.h>
#include<cairo.h>
#include<time.h>
#include<string.h>

xcb_atom_t get_atom_from_name(
		xcb_connection_t *xc, 
		char * name);

xcb_window_t get_manager_selection_owner(
		xcb_connection_t *xc,
		int *reterr);

void draw_clock(
		xcb_connection_t *xc, 
		xcb_window_t win, 
		xcb_visualtype_t *visual_type,
		uint32_t size);

void send_dock_message(
		xcb_connection_t *xc, 
		xcb_window_t win,
		xcb_window_t owner,
		int *sequence_memo);

int main(int argc, char *argv[]){

	int icon_size = 200;

	xcb_generic_event_t * ev;

	//take memo of current seq no.
	int sequence_memo = 0;
	
	//initialize memo of time 
	time_t last_time_draw_seconds = time(NULL);

	//connect to X server
	xcb_connection_t 	*xc;
	xc = xcb_connect(NULL,NULL); //(display name, screen no)
	if(xcb_connection_has_error(xc)){
		printf("X connection error\n");
		return -1;
	}else{
		printf("X connection success\n");
	}

	//get connection setup
	xcb_screen_t 		*screen;
	screen = xcb_setup_roots_iterator( xcb_get_setup(xc) ).data;
	printf("screen root = 0x%x\n", screen->root);

	//obtain visualtype 
	xcb_visualtype_t *visual_type;
	//iterate over all depths
	xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(screen);
	for(;depth_iter.rem;xcb_depth_next(&depth_iter)) {
		xcb_visualtype_iterator_t vis_iter = xcb_depth_visuals_iterator(depth_iter.data);
		for(;vis_iter.rem;xcb_visualtype_next(&vis_iter)) {
			if(screen->root_visual == vis_iter.data->visual_id) {
				visual_type = vis_iter.data;
				break;
			}
		}
	}
	//at this point visual_type contains the correct visual struct
	//I should read the docs of iterator facilities in xcb lol

	//get the owner window of manager selection, 
	xcb_window_t owner;
	int err=0;
	owner = get_manager_selection_owner(xc,&err);

	while(err){
		printf("Tray not found\n");
		sleep(1);
		owner = get_manager_selection_owner(xc,&err);
	}

	//TODO ask the dimension of the available tray window
	

	//gen id for window
	xcb_window_t 
		win = xcb_generate_id(xc);

	printf( "Newly Generated Window ID = 0x%x\n", (int)win );
	

	//create icon window
		//use a block to force short lifetime of these vars
		//prepare masks and values
	uint32_t mask = 
		XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	uint32_t values[2] = {
		screen->black_pixel,
		XCB_EVENT_MASK_EXPOSURE | 
		XCB_EVENT_MASK_BUTTON_PRESS | 
		XCB_EVENT_MASK_PROPERTY_CHANGE |
		XCB_EVENT_MASK_STRUCTURE_NOTIFY
	};

	xcb_create_window(
		xc, 
		XCB_COPY_FROM_PARENT, 
		win, 
		screen->root, 
		0,0, 
		icon_size,icon_size, 
		0, 
		XCB_WINDOW_CLASS_INPUT_OUTPUT, 
		screen->root_visual, 
		mask, values);

	printf("Created New Window\n");

	//set window properties
	xcb_change_property(
			xc,
			XCB_PROP_MODE_REPLACE,
			win,
			get_atom_from_name(xc,"_NET_WM_NAME"),
			get_atom_from_name(xc,"UTF8_STRING"),
			8,
			4,
			"cloc"
			);

	send_dock_message(xc, win, owner, &sequence_memo);
	xcb_flush(xc);

	//show window
	//xcb_map_window(xc,win);
	//xcb_flush(xc);
	//printf("Window map request sent\n");


	//loop while no connection error
	printf("Entering Event loop\n");
	while(xcb_connection_has_error(xc) == 0){

		//Draw clock when 15 seconds passed since last draw
		if(time(NULL) - last_time_draw_seconds > 15 ) {
			draw_clock(xc,win,visual_type,icon_size);
			xcb_flush(xc);
			last_time_draw_seconds = time(NULL);
			printf("15 Seconds Timeout -> redraw at %d\n", (int)last_time_draw_seconds);
		}
		
		ev = xcb_poll_for_event(xc);
		if(ev == NULL) continue;

		print_event_name_from_response_type(ev->response_type);

		switch(ev->response_type & ~0x80){
			case XCB_EXPOSE: {
				xcb_expose_event_t *expose = (xcb_expose_event_t *)ev;
				printf("    H = %d, W = %d\n", expose->height, expose->width);

				if(expose->height > expose->width) 
					icon_size=expose->width;
				else
					icon_size=expose->height;

				draw_clock(xc,win,visual_type,icon_size);
				xcb_flush(xc); 
				last_time_draw_seconds = time(NULL);
			}break;
			case XCB_BUTTON_PRESS: {
				xcb_button_press_event_t *button = (xcb_button_press_event_t *)ev;
				printf("    button=%d, x=%d, y=%d\n", button->detail, button->event_x, button->event_y);

				if( button->detail == 1 ){
					//left mouse click
					//xcb_unmap_window(xc,win); // i don't know why I need to unmap when trying to minimize to systray
					//send_dock_message(xc, win, owner, &sequence_memo);
				}
			}break;
			case XCB_REPARENT_NOTIFY: {
				xcb_reparent_notify_event_t *rep = (xcb_reparent_notify_event_t *)ev;
				printf("    new parent = 0x%x\n",rep->parent);

				//check if wm still exist
				//xcb_window_t check_wm;
				//check_wm = get_manager_selection_owner(xc,&err);
				//printf("    old manager = %d, new manager = %d\n", (int)check_wm, (int)owner);

				if(rep->parent != screen->root) {
					xcb_map_window(xc,win); 
					xcb_flush(xc);
				}

				//if(rep->parent == screen->root){ xcb_map_window(xc,win); xcb_flush(xc); }
			}break;
			case XCB_UNMAP_NOTIFY: {
				send_dock_message(xc, win, owner, &sequence_memo);
				xcb_flush(xc);
		    }break;
			case XCB_CONFIGURE_NOTIFY: {
				xcb_configure_notify_event_t *confnotif = (xcb_configure_notify_event_t *)ev;
				printf("    win = %x\n", confnotif->window);
				printf("    x = %x\n", confnotif->x);
				printf("    y = %x\n", confnotif->y);
				printf("    Width = %x\n", confnotif->width);
				printf("    Height = %x\n", confnotif->height);
				printf("    event = %x\n", confnotif->event);
				printf("    above_sibling = %x\n", confnotif->above_sibling);
			}

		}

		//TODO on click, make another window
		//to show bigger clock and date
		//with close button
		//and "press any button to close" functionality
		


		//flush every loop to make sure things are drawn
		//NOTE: Things did occasionally not drawn if unlucky 
		//(needs to hide-unhide window multiple times to get it drawn sometimes)
		//
		//TODO: Handle signals so we can exit gracefully
		
		free(ev);
	}

	//wrap up
	xcb_disconnect(xc);

	return 0;
}

void print_event_name_from_response_type (int resp) {
	resp = resp & ~0x80;
	printf("[Event] %d:", resp);
	char *name;
	switch( resp ) {
		case 2: name = " XCB_KEY_PRESS "; break;
		case 3: name = " XCB_KEY_RELEASE "; break;
		case 4: name = " XCB_BUTTON_PRESS "; break;
		case 5: name = " XCB_BUTTON_RELEASE  "; break;
		case 6: name = " XCB_MOTION_NOTIFY  "; break;
		case 7: name = " XCB_ENTER_NOTIFY  "; break;
		case 8: name = " XCB_LEAVE_NOTIFY  "; break;
		case 9: name = " XCB_FOCUS_IN  "; break;
		case 10: name = " XCB_FOCUS_OUT  "; break;
		case 11: name = " XCB_KEYMAP_NOTIFY  "; break;
		case 12: name = " XCB_EXPOSE  "; break;
		case 13: name = " XCB_GRAPHICS_EXPOSURE  "; break;
		case 14: name = " XCB_NO_EXPOSURE  "; break;
		case 15: name = " XCB_VISIBILITY_NOTIFY  "; break;
		case 16: name = " XCB_CREATE_NOTIFY  "; break;
		case 17: name = " XCB_DESTROY_NOTIFY  "; break;
		case 18: name = " XCB_UNMAP_NOTIFY "; break;
		case 19: name = " XCB_MAP_NOTIFY "; break;
		case 20: name = " XCB_MAP_REQUEST "; break;
		case 21: name = " XCB_REPARENT_NOTIFY  "; break;
		case 22: name = " XCB_CONFIGURE_NOTIFY  "; break;
		case 23: name = " XCB_CONFIGURE_REQUEST  "; break;
		case 24: name = " XCB_GRAVITY_NOTIFY  "; break;
		case 25: name = " XCB_RESIZE_REQUEST  "; break;
		case 26: name = " XCB_CIRCULATE_NOTIFY  "; break;
		case 27: name = " XCB_CIRCULATE_REQUEST  "; break;
		case 28: name = " XCB_PROPERTY_NOTIFY  "; break;
		case 29: name = " XCB_SELECTION_CLEAR  "; break;
		case 30: name = " XCB_SELECTION_REQUEST  "; break;
		case 31: name = " XCB_SELECTION_NOTIFY  "; break;
		case 32: name = " XCB_COLORMAP_NOTIFY  "; break;
		case 33: name = " XCB_CLIENT_MESSAGE  "; break;
		case 34: name = " XCB_MAPPING_NOTIFY  "; break;
		case 35: name = " XCB_GE_GENERIC "; break;
	}
	printf(name);
	printf("\n");

}

void draw_clock(
		xcb_connection_t *xc, 
		xcb_window_t win, 
		xcb_visualtype_t *visual_type,
		uint32_t size)
{

	//obtaining time using time.h
	uint32_t h;
	uint32_t m;
	{
		time_t raw_time;
		time( &raw_time );
		struct tm *time_struct;
		time_struct = localtime( &raw_time );
		h = time_struct->tm_hour;
		m = time_struct->tm_min;

		//use dummy time
		//h = 10;
		//m = 10;
	}

	//size always assume square
	double center_x = size/2.0;
	double center_y = size/2.0;

	double line_w = 1.5/16.0 * size;

	double h_arm_r = 6.5/16.0 * size;
	double m_arm_r = 6.5/16.0 * size;

	double h_arm_a = 2.0*3.14*( h/12.0 + m/60.0/12.0);
	double m_arm_a = 2.0*3.14*(m/60.0);

	//trasform to cairo's radial coordinate
	h_arm_a -= 3.14/2.0;
	m_arm_a -= 3.14/2.0;

	//prepare cairo
	cairo_surface_t *xc_sfc;

	//create cairo context for xcb
	xc_sfc = cairo_xcb_surface_create( xc, win, visual_type, size, size);
	cairo_t *cr = cairo_create(xc_sfc);

	cairo_set_line_width(cr,line_w);
	cairo_set_antialias(cr,CAIRO_ANTIALIAS_BEST);

	//clear to transparency 
	cairo_set_source_rgba(cr, 0.0,0.0,0.0,0.0); 
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE); 
	cairo_paint(cr); 
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER); 

	//hour arm
	cairo_set_source_rgba(cr, 1.0,1.0,1.0,1.0); //white
	cairo_new_path(cr);
	cairo_arc(cr, center_x, center_y, h_arm_r, h_arm_a, h_arm_a);
	cairo_line_to(cr,center_x,center_y);
	cairo_arc(cr, center_x, center_y, h_arm_r/3.0, h_arm_a+3.14, h_arm_a+3.14);
	cairo_line_to(cr,center_x,center_y);
	cairo_stroke(cr);
	//minute arm
	cairo_set_source_rgba(cr,0.0,0.8,1.0,1.0); //blueish
	cairo_new_path(cr);
	cairo_arc(cr,center_x,center_y,m_arm_r,-3.14/2.0,m_arm_a);
	cairo_stroke(cr);

	//give framing 
	if(1 && (m_arm_a < 1.5*3.14) ){
		cairo_set_source_rgba(cr,0.4,0.3,0.4,1.0); //grayish
		cairo_new_path(cr);
		cairo_arc(cr, center_x, center_y, m_arm_r, m_arm_a,3.0*3.14/2.0);
		cairo_stroke(cr);
	}

	cairo_surface_flush(xc_sfc);
}

xcb_atom_t get_atom_from_name(xcb_connection_t *xc, char * name){
	xcb_intern_atom_cookie_t cookie = 
		//obtain atom of the selection
		xcb_intern_atom(xc,0,strlen(name),name);
	xcb_intern_atom_reply_t *reply = NULL;
	//get reply
	reply = xcb_intern_atom_reply(xc,cookie,NULL);
	xcb_atom_t atom = reply->atom;
	printf("%s = 0x%x0 \n",name, atom);
	return atom;
}

//get the owner window of manager selection, 
//selection name is assuming screen = 0
//TODO obtain the screen number programmatically

//obtain the window ID owning the manager selections

xcb_window_t get_manager_selection_owner(xcb_connection_t *xc,int *reterr){
	xcb_get_selection_owner_cookie_t cookie = 
		xcb_get_selection_owner(
				xc, 
				get_atom_from_name(xc,"_NET_SYSTEM_TRAY_S0"));

	xcb_get_selection_owner_reply_t  *reply = NULL;
	xcb_generic_error_t **error = NULL;

	reply = xcb_get_selection_owner_reply(xc,cookie,error);
	if(error){
		*reterr = 1;
	}

	printf("Manager Selection Owner ID = 0x%x\n", reply->owner);
	return reply->owner;
}

void send_dock_message(
		xcb_connection_t *xc, 
		xcb_window_t win,
		xcb_window_t owner,
		int *sequence_memo) {
	xcb_client_message_event_t dockmsg;
	dockmsg.response_type = XCB_CLIENT_MESSAGE; //
	dockmsg.format = 32; // options: 8, 16, 32
	dockmsg.sequence = *sequence_memo;
	dockmsg.window = win; //icon window ID 
	dockmsg.type = get_atom_from_name(xc,"_NET_SYSTEM_TRAY_OPCODE"); 
	dockmsg.data.data32[0] = XCB_CURRENT_TIME;
	dockmsg.data.data32[1] = 0; //SYSTEM_TRAY_REQUEST_DOCK
	dockmsg.data.data32[2] = win;
	dockmsg.data.data32[3] = 0;
	dockmsg.data.data32[4] = 0;

	xcb_send_event(
			xc,
			0, // propagate = false
			owner,
			XCB_EVENT_MASK_NO_EVENT,
			(const char *)&dockmsg 
			);
	*sequence_memo +=1;
	xcb_flush(xc);
	printf("Wait for reparenting\n");
}


