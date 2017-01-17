package mobile.socket;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;

import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.util.Log;

/**
 * Socket客户端
 * 
 * @author lai_zs
 * @date：2012-3-17 下午12:15:09
 */
public class SocketClient {
	private final String SOCKET_NAME = "htfsk";
	private LocalSocket client;
	private LocalSocketAddress address;
	private boolean isConnected = false;
	private int connetTime = 1;

	public SocketClient() {
		client = new LocalSocket();
		address = new LocalSocketAddress(SOCKET_NAME, LocalSocketAddress.Namespace.RESERVED);
		new ConnectSocketThread().start();
	}

	/**
	 * 发送消息
	 * @param msg
	 * @return 返回Socket服务端的消息回执
	 */
	public String sendMsg(String msg) {
		if (!isConnected) {
			return "Connect fail";
		}
		try {
			BufferedReader in = new BufferedReader(new InputStreamReader(client.getInputStream()));
			PrintWriter out = new PrintWriter(client.getOutputStream());
			out.println(msg);
			out.flush();
			return in.readLine();
		} catch (IOException e) {
			e.printStackTrace();
		}
		return "Nothing return";
	}

	/**
	 * 异步连接Socket,如果连接不上会尝试重复连接十次
	 * 
	 * @author Administrator
	 * 
	 */
	private class ConnectSocketThread extends Thread {
		@Override
		public void run() {
			while (!isConnected && connetTime <= 10) {
				try {
					sleep(1000);
					Log.i("SocketClient","Try to connect socket;ConnectTime:"+connetTime);
					client.connect(address);
					isConnected = true;
				} catch (Exception e) {
					connetTime++;
					isConnected = false;
					Log.i("SocketClient","Connect fail");
				}
			}
		}
	}

	/**
	 * 关闭Socket
	 */
	public void closeSocket() {
		try {
			client.close();
		} catch (IOException e) {
			e.printStackTrace();
		}
	}

}
