#include<stdlib.h>
#include<stdio.h>
#include<xcb/xcb.h>
#include<cairo-xcb.h>
#include<cairo.h>
#include<time.h>
#include<string.h>

xcb_atom_t get_atom_from_name(xcb_connection_t *xc, char * name){
	xcb_intern_atom_cookie_t cookie = 
		//obtain atom of the selection
		xcb_intern_atom(xc,0,strlen(name),name);
	xcb_intern_atom_reply_t *reply;
	//get reply
	reply = xcb_intern_atom_reply(xc,cookie,NULL);
	xcb_atom_t atom = reply->atom;
	//printf("ATOM ID = %u0 \n",atom);
	return atom;
}

int main(int argc, char *argv[]){

	//connect to X server
	xcb_connection_t 	*xc;
	xc = xcb_connect(NULL,NULL); //(display name, screen no)

	//get first screen? 
	xcb_screen_t 		*screen;
	screen = xcb_setup_roots_iterator( xcb_get_setup(xc) ).data;

	//make and show a window
	
		//gen id for window
	xcb_window_t win;
	win = xcb_generate_id(xc);

		//prepare masks and data
	uint32_t mask = 
		XCB_CW_EVENT_MASK ;
	uint32_t values[1] = {
		XCB_EVENT_MASK_EXPOSURE,};

	xcb_create_window(
		xc, 
		XCB_COPY_FROM_PARENT, 
		win, 
		screen->root, 
		0,0, 
		16,16, 
		0, 
		XCB_WINDOW_CLASS_INPUT_OUTPUT, 
		screen->root_visual, 
		mask, values);

	/* setup iconification/systray */
	
	//get the owner window of manager selection, 
	//selection name is assuming screen = 0
	//TODO obtain the screen number programmatically

	//obtain the window ID 
	//owning the manager selections
	xcb_get_selection_owner_cookie_t cookie1 =
		xcb_get_selection_owner(
				xc, 
				get_atom_from_name(xc,"_NET_SYSTEM_TRAY_S0"));

	xcb_get_selection_owner_reply_t  *reply1;
	reply1 = xcb_get_selection_owner_reply(xc,cookie1,NULL);
	xcb_window_t owner = reply1->owner;
	//printf("Selection owner ID = %u0\n", owner);

	xcb_atom_t opcode_atom = get_atom_from_name(xc,"_NET_SYSTEM_TRAY_OPCODE");

	//compose message data 
	//TODO
	xcb_client_message_data_t data;
	data.data32[0] = XCB_CURRENT_TIME;
	data.data32[1] = 0;
	data.data32[2] = win;
	data.data32[3] = 0;
	data.data32[4] = 0;
	
	//compose message of docking
	xcb_client_message_event_t msg;
	msg.response_type = XCB_CLIENT_MESSAGE;
	msg.format = 32;
	msg.sequence = 0; //TODO
	msg.window = win; //check
	msg.type = opcode_atom; //check
	msg.data = data;

	xcb_send_event(
			xc,
			0,
			owner,
			XCB_EVENT_MASK_NO_EVENT,
			(const char *)&msg );

	//draw clock
	
		//get time
		//use dummy time
	uint32_t h = 10;
	uint32_t m = 10;

	double h_arm_r = 5.0;
	double m_arm_r = 7.0;

	double h_arm_a = 2.0*3.14*( h/12.0 + m/60.0/12.0);
	double m_arm_a = 2.0*3.14*(m/60.0);

		//get date
		
		//prepare cairo
	cairo_surface_t *xc_sfc;
	
		//obtain visualtype 
	xcb_visualtype_t *visual_type;
			//iterate over all depths
	xcb_depth_iterator_t 
		depth_iter = xcb_screen_allowed_depths_iterator(screen);
	for(;depth_iter.rem;xcb_depth_next(&depth_iter)) {
		xcb_visualtype_iterator_t 
			vis_iter = xcb_depth_visuals_iterator(depth_iter.data);
		for(;vis_iter.rem;xcb_visualtype_next(&vis_iter)) {
			if(screen->root_visual == vis_iter.data->visual_id) {
				visual_type = vis_iter.data;
				break;
			}
		}
	}
	
		//create cairo context for xcb
	xc_sfc = cairo_xcb_surface_create( xc, win, visual_type, 16, 16);
	cairo_t *cr = cairo_create(xc_sfc);

	cairo_set_line_width(cr,2.0);
	cairo_set_antialias(cr,CAIRO_ANTIALIAS_BEST);

	//show window
	xcb_map_window(xc,win);

	xcb_flush(xc);

		//loop and wait for events forever
		//TODO detect signals 
	
	xcb_generic_event_t *ev;
	while((ev = xcb_wait_for_event(xc))){
		switch(ev->response_type & ~0x80 ){
		case XCB_EXPOSE:
			//printf("Exposed\n");
			cairo_push_group(cr);
			//do redraw every minute for icons
			cairo_set_source_rgb(cr, 1.0,1.0,1.0);
			cairo_new_path(cr);
			cairo_arc(cr,8.0,8.0,h_arm_r,h_arm_a-3.14/2.0,h_arm_a-3.14/2.0);
			cairo_line_to(cr,8.0,8.0);
			cairo_arc(cr,8.0,8.0,h_arm_r/2.0,h_arm_a+3.14/2.0,h_arm_a+3.14/2.0);
			cairo_line_to(cr,8.0,8.0);
			cairo_close_path(cr);
			cairo_stroke(cr);
			//minute arm
			cairo_set_source_rgb(cr,0.0,0.8,1.0);
			cairo_new_path(cr);
			cairo_arc(cr,8.0,8.0,m_arm_r,-3.14/2.0,m_arm_a-3.14/2.0);
			cairo_stroke(cr);
			//give full round for framing 
			cairo_set_source_rgb(cr,0.4,0.3,0.4);
			cairo_new_path(cr);
			cairo_arc(cr,8.0,8.0,m_arm_r,m_arm_a-3.14/2.0,3.0*3.14/2.0);
			cairo_stroke(cr);

			cairo_pop_group_to_source(cr);
			cairo_paint(cr);
			cairo_surface_flush(xc_sfc);
			break;
		default:
			//printf("Do nothing\n");
			break;

			//TODO on click, make another window
			//to show bigger clock and date
			//with close button
			//and "press any button to close" functionality
		}

		//flush every loop to make sure things are drawn
		//NOTE: Things did occasionally not drawn if unlucky 
		//(needs to hide-unhide window multiple times to get it drawn sometimes)
		xcb_flush(xc);
	}


	free(ev);

	//wrap up
	xcb_disconnect(xc);

	return 0;
}
