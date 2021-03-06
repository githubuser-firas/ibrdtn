/*
 * Preferences.java
 * 
 * Copyright (C) 2011 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <morgenroth@ibr.cs.tu-bs.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
package de.tubs.ibr.dtn.daemon;

import java.io.IOException;
import java.net.NetworkInterface;
import java.util.Calendar;
import java.util.Enumeration;

import android.annotation.TargetApi;
import android.app.ActionBar;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.preference.CheckBoxPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceCategory;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.Gravity;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.Switch;
import de.tubs.ibr.dtn.DTNService;
import de.tubs.ibr.dtn.R;
import de.tubs.ibr.dtn.service.DaemonProcess;
import de.tubs.ibr.dtn.service.DaemonService;
import de.tubs.ibr.dtn.stats.CollectorService;

public class Preferences extends PreferenceActivity {
	
	private final String TAG = "Preferences";
	
	private Boolean mBound = false;
	private DTNService service = null;
	
	private Switch actionBarSwitch = null;
	private CheckBoxPreference checkBoxPreference = null;
	
	private ServiceConnection mConnection = new ServiceConnection() {
		public void onServiceConnected(ComponentName name, IBinder service) {
			Preferences.this.service = DTNService.Stub.asInterface(service);
			if (Log.isLoggable(TAG, Log.DEBUG)) Log.d(TAG, "service connected");
				
			// get the daemon version
			try {
			    String version[] = Preferences.this.service.getVersion();
			    setVersion("dtnd: " + version[0] + ", build: " + version[1]);
            } catch (RemoteException e) {
                Log.e(TAG, "Can not query the daemon version", e);
            }
		}

		public void onServiceDisconnected(ComponentName name) {
		    if (Log.isLoggable(TAG, Log.DEBUG)) Log.d(TAG, "service disconnected");
			service = null;
		}
	};
	
	public static void showStatisticLoggerDialog(final Activity activity) {
		DialogInterface.OnClickListener dialogClickListener = new DialogInterface.OnClickListener() {
		    public void onClick(DialogInterface dialog, int which) {
		    	SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(activity);
		        PreferenceActivity prefactivity = (PreferenceActivity)activity;
		        
		        @SuppressWarnings("deprecation")
				CheckBoxPreference cb = (CheckBoxPreference)prefactivity.findPreference("collect_stats");
		    	
		        switch (which){
		        case DialogInterface.BUTTON_POSITIVE:
		        	prefs.edit().putBoolean("collect_stats", true).putBoolean("collect_stats_initialized", true).commit();
		        	cb.setChecked(true);
		            break;

		        case DialogInterface.BUTTON_NEGATIVE:
		        	prefs.edit().putBoolean("collect_stats", false).putBoolean("collect_stats_initialized", true).commit();
		        	cb.setChecked(false);
		            break;
		        }
		    }
		};

		AlertDialog.Builder builder = new AlertDialog.Builder(activity);
		builder.setTitle(R.string.alert_statistic_logger_title);
		builder.setMessage(activity.getResources().getString(R.string.alert_statistic_logger_dialog));
		builder.setPositiveButton(activity.getResources().getString(android.R.string.yes), dialogClickListener);
		builder.setNegativeButton(activity.getResources().getString(android.R.string.no), dialogClickListener);
		builder.show();
	}
	
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
	    MenuInflater inflater = getMenuInflater();
	    inflater.inflate(R.menu.main_menu, menu);
	    
	    if (0 != (getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE)) {
	    	menu.findItem(R.id.itemSendDataNow).setVisible(true);
	    } else {
	    	menu.findItem(R.id.itemSendDataNow).setVisible(false);
	    }
	    
	    return super.onCreateOptionsMenu(menu);
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
	    // Handle item selection
	    switch (item.getItemId()) {        
	    case R.id.itemShowLog:
	    {
			Intent i = new Intent(Preferences.this, LogActivity.class);
			startActivity(i);
	    	return true;
	    }
	    
	    case R.id.itemClearStorage:
	    {
			Intent i = new Intent(Preferences.this, DaemonService.class);
			i.setAction(DaemonService.ACTION_CLEAR_STORAGE);
			startService(i);
	    	return true;
	    }
	    
	    case R.id.itemSendDataNow:
	    {
	    	SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
			Calendar now = Calendar.getInstance();
			prefs.edit().putLong("stats_timestamp", now.getTimeInMillis()).commit(); 

			// open activity
			Intent i = new Intent(this, CollectorService.class);
			i.setAction(CollectorService.DELIVER_DATA);
			startService(i);
	    	return true;
	    }
	    
	    case R.id.itemApps:
	    {
			Intent i = new Intent(Preferences.this, AppListActivity.class);
			startActivity(i);
	    	return true;
	    }
	     
	    case R.id.itemNeighbors:
	    {
	    	// open neighbor list activity
	    	Intent i = new Intent(Preferences.this, NeighborActivity.class);
	    	startActivity(i);
	    	return true;
	    }
	    
	    default:
	        return super.onOptionsItemSelected(item);
	    }
	}
	
	public static void initializeDefaultPreferences(Context context) {
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
		
		if (prefs.getBoolean("initialized", false)) return;

		Editor e = prefs.edit();
		e.putString("endpoint_id", DaemonProcess.getUniqueEndpointID(context).toString());
		
		try {
			// scan for known network devices
			for(Enumeration<NetworkInterface> list = NetworkInterface.getNetworkInterfaces(); list.hasMoreElements();)
		    {
	            NetworkInterface i = list.nextElement();
	            String iface = i.getDisplayName();
	            
	            if (	iface.contains("wlan") ||
	            		iface.contains("wifi") ||
	            		iface.contains("eth")
	            	) {
	            	e.putBoolean("interface_" + iface, true);
	            }
		    }
		} catch (IOException ex) { }
		
		// set preferences to initialized
		e.putBoolean("initialized", true);
		
		e.commit();
	}

	@TargetApi(14)
	@SuppressWarnings("deprecation")
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		// set default preference values
		PreferenceManager.setDefaultValues(this, R.xml.preferences, false);
		
		// initialize default values if configured set already
		initializeDefaultPreferences(this);

		addPreferencesFromResource(R.xml.preferences);
		
		// connect daemon controls
        checkBoxPreference = (CheckBoxPreference) findPreference("enabledSwitch");
        if (checkBoxPreference == null) {
			// use custom actionbar switch
	        actionBarSwitch = new Switch(this);

	        //PreferenceActivity preferenceActivity = (PreferenceActivity) this;
	        //if (preferenceActivity.onIsHidingHeaders() || !preferenceActivity.onIsMultiPane()) {
	            final int padding = this.getResources().getDimensionPixelSize(
	                    R.dimen.action_bar_switch_padding);
	            actionBarSwitch.setPadding(0, 0, padding, 0);
	            this.getActionBar().setDisplayOptions(ActionBar.DISPLAY_SHOW_CUSTOM,
	                    ActionBar.DISPLAY_SHOW_CUSTOM);
	            this.getActionBar().setCustomView(actionBarSwitch, new ActionBar.LayoutParams(
	                    ActionBar.LayoutParams.WRAP_CONTENT,
	                    ActionBar.LayoutParams.WRAP_CONTENT,
	                    Gravity.CENTER_VERTICAL | Gravity.RIGHT));
	        //}
	        
	        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(Preferences.this);

	        // read initial state of the switch
	        actionBarSwitch.setChecked( prefs.getBoolean("enabledSwitch", false) );
	        
	        actionBarSwitch.setOnCheckedChangeListener(new OnCheckedChangeListener() {
				public void onCheckedChanged(CompoundButton arg0, boolean val) {

					if (val) {
						// set "enabledSwitch" preference to true
						SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(Preferences.this);
						prefs.edit().putBoolean("enabledSwitch", true).commit();
					}
					else
					{
						// set "enabledSwitch" preference to false
						SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(Preferences.this);
						prefs.edit().putBoolean("enabledSwitch", false).commit();
					}
				}
	        });
		}
		
		// list all network interfaces
		try {
			PreferenceCategory pc = (PreferenceCategory) findPreference("prefcat_interfaces");
			
			for(Enumeration<NetworkInterface> list = NetworkInterface.getNetworkInterfaces(); list.hasMoreElements();)
		    {
	            NetworkInterface i = list.nextElement();
	            
	            // skip virtual interfaces
	            if (i.isVirtual()) continue;
	            
	            // do not work on non-multicast interfaces
	            if (!i.supportsMulticast()) continue;
	            
	            // skip loopback device
	            if (i.isLoopback()) continue;
	            
	            // skip rmnet
	            if (i.getDisplayName().startsWith("rmnet")) continue;
	            
	            String iface = i.getDisplayName();
	            CheckBoxPreference cb_i = new CheckBoxPreference(this);
	            
	            cb_i.setTitle(iface);
	            
	            if (i.isPointToPoint())
	            {
	            	cb_i.setSummary("Point-to-Point");
	            }
	            else if (i.isLoopback())
	            {
	            	cb_i.setSummary("Loopback");
	            }
	            
	            cb_i.setKey("interface_" + iface);
	            pc.addPreference(cb_i);
	            cb_i.setDependency(pc.getDependency());
		    }
		} catch (IOException e) { }
		
		// set initial version
		setVersion(null);
	}
	
	@Override
	public void onDestroy() {
	    if (mBound) {
	        // Detach our existing connection.
	        unbindService(mConnection);
	        mBound = false;
	    }

	    super.onDestroy();
	}
	
	@SuppressWarnings("deprecation")
	private void setVersion(String versionValue) {
        // version information
        Preference version = findPreference("system_version");
        try {
            PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), 0);
            if (versionValue == null) {
                version.setSummary("app: " + info.versionName);
            } else {
                version.setSummary("app: " + info.versionName + ", " + versionValue);
            }
        } catch (NameNotFoundException e) { };
	}
	
    @Override
	protected void onPause() {
		super.onPause();
	}

	@Override
	protected void onResume() {
	    if (!mBound) {
    		// Establish a connection with the service.  We use an explicit
    		// class name because we want a specific service implementation that
    		// we know will be running in our own process (and thus won't be
    		// supporting component replacement by other applications).
    		bindService(new Intent(Preferences.this, 
    				DaemonService.class), mConnection, Context.BIND_AUTO_CREATE);
    		mBound = true;
	    }
  		
		super.onResume();
		
		// on first startup ask for permissions to collect statistical data
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(Preferences.this);
		if (!prefs.getBoolean("collect_stats_initialized", false)) {
			showStatisticLoggerDialog(Preferences.this);
		}
	}
}
