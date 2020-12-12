/* Autogenerated by png2wx.pl on Fri Dec  1 10:51:37 2017 */
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#	include <wx/wx.h>
#endif
#include <wx/filename.h>
#include <wx/mstream.h>
#include "icons.h"

wxBitmap *_img_EarthExplorerIcon;
bool m_managedPlugin;


#ifdef PLUGIN_USE_SVG
#include "ocpn_plugin.h"
wxString _svg_earthexplorer;
wxString _svg_earthexplorer_toggled;
#endif

void initialize_images(void)
{
	{
		wxMemoryInputStream sm("\211PNG\r\n\032\n\000\000\000\rIHDR\000\000\000 \000\000\000 \b\003\000\000\000D\244\212\306\000\000\000\004gAMA\000\000\261\217\013\374a\005\000\000\000 cHRM\000\000z&\000\000\200\204\000\000\372\000\000\000\200\350\000\000u0\000\000\352`\000\000:\230\000\000\027p\234\272Q<\000\000\000\311PLTE\000\000\000\000\000\000\003\000\003\003\002\003\003\001\004\004\001\004\003\001\004\000\000\000\003\002\003\003\001\004\000\000\000\004\002\004\003\001\004\000\000\000\003\001\004\006\000\006\003\001\004\003\001\004\003\002\003\003\000\005\004\000\004\000\000\000\003\001\004\004\002\004\000\000\000\003\001\004\000\000\000\003\001\004\000\000\000\003\001\004\003\001\004\000\000\000\004\002\004\003\001\004\000\000\000\003\001\004\005\000\005\005\000\005\003\000\003\003\002\003\003\001\003\004\000\004\002\000\004\003\001\004\002\000\005\004\000\004\003\001\004\003\002\003\000\000\000\003\001\004\000\000\000\003\000\005\004\002\004\000\000\000\002\001\004\003\001\004\000\000\a\000\000\000\003\001\004\003\001\004\004\001\004\000\000\a\002\000\005\002\000\004\003\001\004\003\001\004\000\000\000L\251+\351\000\000\000AtRNS\000\aI\230\301\332\363\031\227\362\b\204\365\030\310-\343\356\222f=\024\361\200\017\274\021\272\011\273\367\034\210\355\020\30365]\234\333Du\374qC\353\237\036\350\nc\205\002\316\371&\003\313\267\326#pt\271m\262\370e\000\000\000\001bKGD\000\210\005\035H\000\000\000\011pHYs\000\000\r\327\000\000\r\327\001B(\233x\000\000\000\atIME\a\341\014\001\013\034!\312\313J\001\000\000\001lIDAT8\313\205S\327\266\202@\014\214tA\257\250\330+\212\242\"\366\336\371\377\237\272\233\200\b\350\321\274\220L\302&\231\235\005\b-\305\361\202(I\242\300s)x7YI{\241\245\0259\221V\265\214\027\263\214\246F\363\331?\357\315\376\262\257|N'(_(\032\245\222Q,\344)\324s\341\377\224/W\252O\240Z)SEp\206J\347\327\352\321\236\365\032u\361\347\320\320o4\343S7\033\210j\264\037\316\337R!aj\013w\301m\025\346\264;\357\304t\332,\2410\376\220\237\356\a\346\240\213\214\245\200c\037\263\367\251\240g\262\024\a\274\367\325x\020\276\027\b \306\342\276e\365c\200\bR,\266\000\254\030 Q\301`h\217\222\005#{8\240\002l1f3O\234h\013g\302\2401\265\300!\247\270\225;{\035<s\021\231\322\220\270\346\234\366^,\237\371\345\202\2009\255\211D\351+\002\326\317\2025\205+\235\210\"\2527\204lw~~\267\245p\343SM\227\265?\020d\370\005\006\005\207\275\177Y\301u\037\011<a\376D\3561\274n_0g\322\233\215\256M\252;\207\202\011$w\2712\367\206\336\2159\327KDr\201h\315\273\014.:.\310w3*\332P\366\316\003\251\361\246\017'!\373\337\017\347\367\323\373\375x\321>?\377\177\034\207\177\335C\030\372\a\000\000\000%tEXtdate:create\0002017-12-01T11:28:33+01:00\254\342vt\000\000\000%tEXtdate:modify\0002017-12-01T11:28:33+01:00\335\277\316\310\000\000\000\031tEXtSoftware\000www.inkscape.org\233\356<\032\000\000\000\000IEND\256B`\202", 959);
		_img_EarthExplorerIcon = new wxBitmap(wxImage(sm));
	}

#ifdef MANAGED_PLUGIN
	m_managedPlugin = true;
#else
	m_managedPlugin = false;
#endif

#ifdef PLUGIN_USE_SVG
    wxFileName fn;
	wxString tmp_path;
	if (m_managedPlugin) {
		tmp_path = GetPluginDataDir("earthexplorer_pi");
		fn.SetPath(tmp_path);
		fn.AppendDir(_T("data"));
	}
	else {
		fn.SetPath(*GetpSharedDataLocation());
		fn.AppendDir(_T("plugins"));
		fn.AppendDir(_T("earthexplorer_pi"));
		fn.AppendDir(_T("data"));
	}

    fn.SetFullName(_T("earthexplorer_pi.svg"));
    _svg_earthexplorer = fn.GetFullPath();
    fn.SetFullName(_T("earthexplorer_pi_toggled.svg"));
    _svg_earthexplorer_toggled = fn.GetFullPath();
#endif

	return;
}


