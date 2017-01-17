package mobile.socket;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.View;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

public class MySocketActivity extends Activity {
	private SocketClient socketClient = null;
	private static String INFO;

	/** Called when the activity is first created. */
	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);
		Log.i("MySocketActivity", "Sleep waiting for service socket start");
		socketClient = new SocketClient();
	}

	/**
	 * ·¢ËÍÐÕÃû
	 * 
	 * @param view
	 */
	public void sendName(View view) {
		String name = ((TextView) this.findViewById(R.id.editText1)).getText().toString();
		if (name == "") {
			Toast.makeText(this, getResources().getString(R.string.nameNull), Toast.LENGTH_SHORT).show();
			return;
		}
		String sex = ((Spinner) this.findViewById(R.id.spinner1)).getSelectedItem().toString();
		INFO = sex+ "." + name ;
		new SocketThread(handler).start();
	}

	private class SocketThread extends Thread {
		private Handler handler;

		public SocketThread(Handler handler) {
			super();
			this.handler = handler;
		}

		@Override
		public void run() {
			Log.i("MySocketActivity", "SocketThead wait for result.");
			String result = getSocketClient().sendMsg(INFO);
			Log.i("MySocketActivity", "Socket reslut:" + result);
			Message msg = handler.obtainMessage();
			Bundle bundle = new Bundle();
			bundle.putString("result", result);
			msg.setData(bundle);
			handler.sendMessage(msg);
		}
	}

	private Handler handler = new Handler() {
		public void handleMessage(android.os.Message msg) {
			String result = msg.getData().getString("result");
			Log.i("MySocketActivity", "Handler result:" + result);
			((TextView) MySocketActivity.this.findViewById(R.id.tvResult)).setText(result);
		};
	};

	@Override
	protected void onDestroy() {
		if (socketClient != null) {
			getSocketClient().closeSocket();
		}
		super.onDestroy();
	}

	public SocketClient getSocketClient() {
		if (socketClient == null) {
			socketClient = new SocketClient();
		}
		return socketClient;
	}

}