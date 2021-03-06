#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>
#include <GL/gl.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include "XWindow.h"
#include "XDisplay.h"


class GrabServer
{
public:
	Display * _dpy;
	GrabServer(Display * dpy)
	{
		_dpy = dpy;
		XGrabServer(dpy);
	}
	~GrabServer()
	{
		XUngrabServer(_dpy);
	}
};

int GetTime();

XWindow::XWindow(Display * dpy, Window w, XWindow * next) : _matrix(Matrix::identity)
{
	_dpy = dpy;
	_w = w;
	_next = next;
	_parent = NULL;
	_sibling = NULL;
	_nchildren = 0;
	_children = NULL;
	_name = NULL;
	_texture = false;
	_mapped = false;
	_width = 0;
	_height = 0;
}

XWindow::~XWindow()
{
	Unmap();
}

void XWindow::Add(XWindow * new_child)
{
	XWindow * child;
	if (_children)
	{
		for (child = _children; child->_sibling; child = child->_sibling)
		{
			if (child == new_child)
			{
				return;
			}
		}
		if (child == new_child)
		{
			return;
		}
		new_child->_parent = this;
		new_child->_sibling = NULL;
		_nchildren++;
		child->_sibling = new_child;
	}
	else
	{
		new_child->_parent = this;
		new_child->_sibling = NULL;
		_nchildren++;
		_children = new_child;
	}
}

void XWindow::Remove(XWindow * rem_child)
{
	XWindow * prev, *child;
	if (!_children)
	{
		return;
	}
	if (_children == rem_child)
	{
		// assert _parent == this
		_children = rem_child->_sibling;
		rem_child->_parent = NULL;
		return;
	}
	prev = _children;
	child = _children->_sibling;
	while (child && child != rem_child)
	{
		prev = child;
		child = child->_sibling;
	}
	if (!child)
	{
		return;
	}
	prev->_sibling = child->_sibling;
	child->_parent = NULL;
}

bool XWindow::Initialize()
{
	XWindowAttributes attrib;
	if (!XGetWindowAttributes(_dpy, _w, &attrib))
	{
		printf(" unabled to get window attributes\n");
		return false;
	}

	XFetchName(_dpy, _w, &_name);

	_x = attrib.x;
	_y = attrib.y;
	_width = attrib.width;
	_height = attrib.height;

	_matrix = *(Matrix*)Matrix::identity;
	_matrix.translation()._x += attrib.x;
	_matrix.translation()._y -= attrib.y;
	
	_event_mask = attrib.all_event_masks;
	// attrib.override_redirect can indicate popup window!
	
	printf("0x%08x rect %d %d %d %d (%d) root %08x class %d, gravity bit %d win %d, backing store %d planes %d pixels %d, save under %d, map %d, events %x, do not prop %x, override %d\n",
			(int)_w,
			attrib.x, attrib.y, attrib.width, attrib.height, attrib.depth,
			(int)attrib.root, attrib.c_class, attrib.bit_gravity, attrib.win_gravity,
			attrib.backing_store, (int)attrib.backing_planes, (int)attrib.backing_pixel,
			attrib.save_under, attrib.map_state, (int)attrib.all_event_masks, (int)attrib.do_not_propagate_mask,
			attrib.override_redirect);

	if (attrib.root != _w)
	{
		if (attrib.c_class == InputOutput)
		{
			XDamageCreate (_dpy, _w, XDamageReportRawRectangles);
		}
	    XSelectInput (_dpy, _w, SubstructureNotifyMask | FocusChangeMask | ExposureMask);
	}

#if 0
	if (attrib.map_state == IsViewable)
	{
		Update(0,0,0,0);
	}
#endif

#if 0
	if (attrib.root == _w)
	{
		Window *children, dummy;
		unsigned int nchildren;
		if (!XQueryTree(_dpy, _w, &dummy, &dummy, &children, &nchildren))
		{
			return true;
		}

		for (int i=nchildren - 1; i >= 0; i--)
		{
			XWindow * child = GetWindow(_dpy, children[i]);
			if (!child)
			{
				continue;
			}
			child->_parent = this;
			child->_sibling = _children;
			_nchildren++;
			_children = child;
		}
	}
#endif
	return true;
}

void XWindow::UpdateHierarchy()
{
	Window *children, dummy;
	unsigned int nchildren;

	if (!XQueryTree(_dpy, _w, &dummy, &dummy, &children, &nchildren))
	{
		return;
	}

	_hdepth = 0;
	for (XWindow * parent = _parent; parent; parent = parent->_parent)
	{
		_hdepth++;
	}

	//for (int i=nchildren - 1; i >= 0; i--)
	for (int i=0; i < nchildren; i++)
	{
		XWindow * child = XDisplay::GetWindow(_dpy, children[i]);
		if (!child)
		{
			continue;
		}
		Add(child);
		child->UpdateHierarchy();
	}
}

bool XWindow::Update(int x, int y, int width, int height)
{
	GrabServer grab(_dpy);

	XWindowAttributes attrib;
	if (!XGetWindowAttributes(_dpy, _w, &attrib))
	{
		printf(" unabled to get window attributes\n");
		return false;
	}

	_mapped = attrib.map_state == IsViewable;

	if (_hdepth != 1)
	{
		return true;
	}

	if (!_textured)
	{
		if (attrib.map_state == IsViewable && attrib.c_class == InputOutput)
		{
			x = 0;
			y = 0;
			width = attrib.width;
			height = attrib.height;
			_width = 0;
			_height = 0;
			glGenTextures(1, &_texture);
			_textured = true;
		}
		else
		{
			return true;
		}
	}
	else
	{
		if (attrib.map_state != IsViewable)
		{
			Unmap();
			return true;
		}
	}

	glBindTexture(GL_TEXTURE_2D, _texture);
	if (_width != attrib.width || _height != attrib.height)
	{
		x = 0;
		y = 0;
		width = attrib.width;
		height = attrib.height;
		_width = attrib.width;
		_height = attrib.height;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, _width, _height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
	}
	else
	{
		int x2 = x + width;
		int y2 = y + height;
		// crop
		if (x > _width || y > _height || x2 <= 0 || y2 <= 0)
		{
			return true;
		}
		// clip
		if (x < 0)
		{
			width += x;
			x = 0;
		}
		if (y < 0)
		{
			height += y;
			y = 0;
		}
		if (x2 > _width)
		{
			width = _width - x;
		}
		if (y2 > _height)
		{
			height = _height - y;
		}
	}

	XImage *image = XGetImage (_dpy, _w, x, y, width, height, AllPlanes, ZPixmap);
	if (!image)
	{
		printf(" unabled to get the image\n");
		return false;
	}

    int bytes_per_pixel = image->bits_per_pixel / 8;
    unsigned char * texture = (unsigned char *)malloc(width * height * bytes_per_pixel);
    unsigned char * dst = texture;
    for ( int py = 0; py < height; py++)
    {
        unsigned char * src = ((unsigned char*)image->data) + (image->bytes_per_line * py);
        for ( int px = 0; px < width; px++)
        {
            unsigned char t;
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            if (bytes_per_pixel > 3)
                dst[3] = 255;
            src += bytes_per_pixel;
            dst += bytes_per_pixel;
        }
    }

    if (bytes_per_pixel == 4)
    {
        glTexSubImage2D( GL_TEXTURE_2D, 0, x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, texture);
    }
    else if (bytes_per_pixel == 3)
    {
        glTexSubImage2D( GL_TEXTURE_2D, 0, x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, texture);
    }
    else
    {
        printf("depth %d\n", image->depth);
    }
	free(texture);

    XDestroyImage(image);

	return true;
}

void XWindow::Unmap()
{
	if (!_mapped)
	{
		return;
	}
	if (_textured)
	{
		glDeleteTextures(1, &_texture);
		_textured = false;
	}
	_mapped = false;
}

XWindow * XWindow::GetEventWindow(int event_mask, int &x, int &y)
{
	XWindow * child = NULL;
	int cx, cy;
	//printf("GetEventWindow %08x %d %d\n", (int)_w, x, y); 
	for (XWindow * w = _children; w; w = w->_sibling)
	{
		if (!w->_mapped /*|| !(w->_event_mask & event_mask)*/)
		{
			continue;
		}

		int wx = x - w->_x;
		int wy = y - w->_y;
		if (wx < 0 || wy < 0 || wx >= w->_width || wy >= w->_height)
		{
			//printf("  child %08x FAIL %d %d %d %d\n", (int)w->_w, w->_x, w->_y, w->_width, w->_height); 
			continue;
		}

		//printf("  child %08x SUCCESS %d %d\n", (int)w->_w, wx, wy); 
		child = w;
		cx = wx;
		cy = wy;
	}
	if (child)
	{
		x = cx;
		y = cy;
		return child->GetEventWindow(event_mask, x, y);
	}
	return this;
}

void XWindow::Draw()
{
	float w = _width;
	float h = _height;
	float vertices[] =
	{
		0.f, 0.f, 0.f, 0.f,
		w, 0.f, 1.f, 0.f,
		w, -h, 1.f, 1.f,
		0.f, -h, 0.f, 1.f
	};

	glPushMatrix();
	glMultMatrixf(_matrix._m);

	if (_textured)
	{
		glColor4f(1.0, 1.0, 1.0, 1.0);
		glBindTexture(GL_TEXTURE_2D, _texture);

		glDisable(GL_BLEND);
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_TEXTURE_2D);
		glClientActiveTexture(GL_TEXTURE0);
		glActiveTexture(GL_TEXTURE0);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);

		glVertexPointer(2, GL_FLOAT, 4*4, vertices );
		glTexCoordPointer(2, GL_FLOAT, 4*4, vertices + 2 );

		glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

	if (_hdepth == 0)
	{
		for (XWindow * child = _children; child; child = child->_sibling)
		{
			child->Draw();
		}
	}

	glPopMatrix();
}

bool XWindow::IsParent(XWindow * w)
{
	for (XWindow * parent = _parent; parent; parent = parent->_parent)
	{
		if (w == parent)
		{
			return true;
		}
	}
	return false;
}

void XWindow::SendMotionEvent(Window root, int x, int y, int state)
{
	int x_root = x + _x;
	int y_root = y + _y;
	for (XWindow * parent = _parent; parent; parent = parent->_parent)
	{
		x_root += parent->_x;
		y_root += parent->_y;
	}
	XEvent event;
	event.xmotion.type = MotionNotify;
	event.xmotion.display = _dpy;
	event.xmotion.window = _w;
	event.xmotion.root = root;
	event.xmotion.subwindow = None;
	event.xmotion.time = GetTime(); //CurrentTime;
	event.xmotion.x = x;
	event.xmotion.y = y;
	event.xmotion.x_root = x_root;
	event.xmotion.y_root = y_root;
	event.xmotion.state = state;
	event.xmotion.is_hint = 0;
	event.xmotion.same_screen = True;
	XSendEvent(_dpy, _w, True, PointerMotionMask, &event);
	XFlush(_dpy);
}

void XWindow::SendCrossingEvent(Window root, int x, int y, int state, int detail, Window child, bool enter)
{
	int x_root = x + _x;
	int y_root = y + _y;
	for (XWindow * parent = _parent; parent; parent = parent->_parent)
	{
		x_root += parent->_x;
		y_root += parent->_y;
	}
	XEvent event;
	event.xcrossing.type = enter? EnterNotify : LeaveNotify;
	event.xcrossing.display = _dpy;
	event.xcrossing.window = _w;
	event.xcrossing.root = root;
	event.xcrossing.subwindow = child; // None
	event.xcrossing.time = GetTime();//CurrentTime;
	event.xcrossing.x = x;
	event.xcrossing.y = y;
	event.xcrossing.x_root = x_root;
	event.xcrossing.y_root = y_root;
	event.xcrossing.mode = NotifyNormal;
	event.xcrossing.detail = detail;//enter? NotifyNonlinear : NotifyAncestor;
	event.xcrossing.same_screen = True;
	event.xcrossing.focus = True;//enter? True : False;
	event.xcrossing.state = state;
	XSendEvent(_dpy, _w, True, enter? EnterWindowMask : LeaveWindowMask, &event);
	XFlush(_dpy);
}

void XWindow::SendButtonEvent(Window root, int x, int y, int button, int state, bool press)
{
	int x_root = x + _x;
	int y_root = y + _y;
	for (XWindow * parent = _parent; parent; parent = parent->_parent)
	{
		x_root += parent->_x;
		y_root += parent->_y;
	}

	XEvent event;
	memset(&event, 0, sizeof(event));

	event.xbutton.type = press? ButtonPress : ButtonRelease;
	event.xbutton.display = _dpy;
	event.xbutton.window = _w;
	event.xbutton.root = root;
	event.xbutton.subwindow = None;
	event.xbutton.time = GetTime();//CurrentTime;
	event.xbutton.x = x;
	event.xbutton.y = y;
	event.xbutton.x_root = x_root;
	event.xbutton.y_root = y_root;
	event.xbutton.state = state;
	event.xbutton.button = button;
	event.xbutton.same_screen = True;

	XSendEvent(_dpy, _w, True, press? ButtonPressMask : ButtonReleaseMask, &event);
	XFlush(_dpy);
}

void XWindow::SendKeyEvent(Window root, int key, int state, bool press)
{
	XKeyEvent event;
	event.type = press? KeyPress : KeyRelease;
	event.display = _dpy;
	event.window = _w;
	event.root = root;
	event.subwindow = None;
	event.time = GetTime(); //CurrentTime;
	event.x = 1;
	event.y = 1;
	event.x_root = 1;
	event.y_root = 1;
	event.same_screen = True;
	event.keycode = key;
	event.state = state;
	XSendEvent(_dpy, _w, True, press? KeyPressMask : KeyReleaseMask, (XEvent*)&event);
	XFlush(_dpy);
}

