using GMap.NET;
using GMap.NET.MapProviders;
using GMap.NET.WindowsForms;
using GMap.NET.WindowsForms.Markers;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using uPLibrary.Networking.M2Mqtt;
using uPLibrary.Networking.M2Mqtt.Messages;

namespace MQTT_Utility
{
    public partial class Form1 : Form
    {
        private MqttClient client;
        public delegate void MyInvoke(byte[] data, string topic);
        public int save_flag = 0;
        public string save_path = "";
        public Form1()
        {
            InitializeComponent();
        }

        private void Form1_Load(object sender, EventArgs e)
        {

        }
        void MqttAWSInit()
        {
            //pfx需要安裝到電腦中.
            var broker = "a22gkkuykn8yvq-ats.iot.ap-northeast-1.amazonaws.com"; //<AWS-IoT-Endpoint>           
            int port = 8883;
            string certPass = "123456";
            //certificates Path
            //var certificatesPath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "certs");

            //var caCertPath = Path.Combine(certificatesPath, "AmazonRootCA1.pem");
            var caCert = X509Certificate.CreateFromCertFile("AmazonRootCA1.pem");

            //var deviceCertPath = Path.Combine(certificatesPath, "certificate.cert.pfx");
            var deviceCert = new X509Certificate("certificate.cert.pfx", certPass);

            // Create a new MQTT client.
            client = new MqttClient(broker, port, true, caCert, deviceCert, MqttSslProtocols.TLSv1_2);

            // 注册消息接收处理事件，还可以注册消息订阅成功、取消订阅成功、与服务器断开等事件处理函数  
            client.MqttMsgPublishReceived += client_MqttMsgPublishReceived;

            string clientId = Guid.NewGuid().ToString();
            //string clientId = textBox3.Text;
            client.Connect(clientId);
            // 订阅主题"/home/temperature" 消息质量为 2   
            client.Subscribe(new string[] { textBox6.Text }, new byte[] { MqttMsgBase.QOS_LEVEL_AT_LEAST_ONCE});
            //client.Publish(textBox6.Text, Encoding.UTF8.GetBytes("hello world"));
            MessageBox.Show("connect ok");
        }
        /*void MqttServerInit()
        {
            
            X509Certificate caCertificate = new X509Certificate("mosquitto.org.crt");
            //X509Certificate caCertificate = X509Certificate.CreateFromCertFile("mosquitto.org.crt");
            //client = new MqttClient(textBox2.Text); //当主机地址为域名时
            client = new MqttClient(textBox2.Text, int.Parse(textBox22.Text), false, caCertificate, null,MqttSslProtocols.TLSv1_2);
            // 注册消息接收处理事件，还可以注册消息订阅成功、取消订阅成功、与服务器断开等事件处理函数  
            client.MqttMsgPublishReceived += client_MqttMsgPublishReceived;
            //生成客户端ID并连接服务器  
            string clientId = Guid.NewGuid().ToString();
            //string clientId = textBox3.Text;
            client.Connect(clientId,textBox3.Text,textBox4.Text);

            // 订阅主题"/home/temperature" 消息质量为 2   
            client.Subscribe(new string[] {textBox6.Text }, new byte[] { MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE });
        }*/
        void client_MqttMsgPublishReceived(object sender, MqttMsgPublishEventArgs e)
        {
            //处理接收到的消息  

            //string msg = System.Text.Encoding.Default.GetString(e.Message);
            string topic = e.Topic;
            MyInvoke mi = new MyInvoke(UpdateForm);
            try
            {
                this.BeginInvoke(mi, new Object[] { e.Message, topic });
            }
            catch (Exception ex) {; }
            //MessageBox.Show(msg);
            //textBox1.AppendText("收到消息:" + msg + "\r\n");
        }

        public void ReceivedDataConvert()
        {
            
            //MessageBox.Show("false");
            string str = textBox1.Text;
            textBox1.Text = "";
            string[] token = str.Split(' ');
            foreach (string hex in token)
            {
                try
                {
                    int val = int.Parse(hex, System.Globalization.NumberStyles.HexNumber);
                    textBox1.Text += Encoding.ASCII.GetString(new byte[] { (Byte)val });
                }
                catch (Exception ex)
                {
                    ;
                }
            }
        }

        public void UpdateForm(byte[] data, string topic)
        {
            
            string str_data = System.Text.Encoding.Default.GetString(data);
            string msg = "from:" + topic + ":" + str_data;
            textBox1.Text += msg;
            textBox2.Text += msg;
            toolStripStatusLabel1.Text = "Last update time:" + DateTime.Now.Year + "/" + DateTime.Now.Month + "/" + DateTime.Now.Day + " " + DateTime.Now.Hour + ":" + DateTime.Now.Minute + ":" + DateTime.Now.Second;
            if (str_data.Contains("+EVT_STATUS") == true)
            {
                string[] mysplit = str_data.Split(',');
                double lat, longt;
                try
                {
                    lat = Convert.ToDouble(mysplit[4].Replace('N', ' '));
                    longt = Convert.ToDouble(mysplit[5].Replace('E', ' '));
                }
                catch (Exception ex)
                {
                    lat = longt = 0;
                }
                Mapping2Map(lat, longt);
                label7.Text = "ON";
                label7.ForeColor = Color.Green;
                //label12.Text = "driving";
                label9.Text = mysplit[1] + "%";
                label10.Text = mysplit[2] + "KM";
                if (mysplit[3].Contains("0"))
                {
                    label8.Text = "Close";
                    label8.ForeColor = Color.Red;
                }
                else
                {
                    label8.Text = "Open";
                    label8.ForeColor = Color.Green;
                }
            }
            else if (str_data.Contains("OK+GetStatus") == true)
            {
                string[] mysplit = str_data.Split(',');
                double lat = Convert.ToDouble(mysplit[4].Replace('N', ' '));
                double longt = Convert.ToDouble(mysplit[5].Replace('E', ' '));
                Mapping2Map(lat, longt);
                //label7.Text = "ON";
                //label12.Text = "driving";
                label9.Text = mysplit[1] + "%";
                label10.Text = mysplit[2] + "KM";
                if (mysplit[3].Contains("0"))
                {
                    label8.Text = "Close";
                    label8.ForeColor = Color.Red;
                }
                else
                {
                    label8.Text = "Open";
                    label8.ForeColor = Color.Green;
                }
            }
            else if (str_data.Contains("+EVT_ACC_") == true)
            {
                string[] mysplit = str_data.Split(',');
                label9.Text = mysplit[1] + "%";
                label10.Text = mysplit[2] + "KM";
                if (mysplit[3].Contains("0"))
                {
                    label8.Text = "Close";
                    label8.ForeColor = Color.Red;
                }
                else
                {
                    label8.Text = "Open";
                    label8.ForeColor = Color.Green;
                }
                if (str_data.Contains("+EVT_ACC_ON") == true)
                {
                    label7.Text = "ON";
                    label7.ForeColor = Color.Green;
                }
                else
                {
                    label7.Text = "OFF";
                    label7.ForeColor = Color.Red;
                }

            }
            else if (str_data.Contains("+EVT_ARM") == true)
            {
                label12.Text = "ARM";
                label12.ForeColor = Color.Red;
            }
            else if (str_data.Contains("+EVT_DISARM") == true)
            {
                label12.Text = "DISARM";
                label12.ForeColor = Color.Green;
            }
        }

        private void button1_Click(object sender, EventArgs e)
        {
            //MqttServerInit();
            MqttAWSInit();
        }

        private void button2_Click(object sender, EventArgs e)
        {
            client.Disconnect();
        }

        private void button4_Click(object sender, EventArgs e)
        {
            textBox1.Text = "";
        }

        private void button5_Click(object sender, EventArgs e)
        {
            string str = textBox5.Text;
            client.Publish(textBox7.Text, Encoding.UTF8.GetBytes(str));

            
        }

        private void checkBox2_CheckedChanged(object sender, EventArgs e)
        {
            
            //MessageBox.Show("false");
            string str = textBox5.Text;
            textBox5.Text = "";
            string[] token = str.Split(' ');
            foreach (string hex in token)
            {
                try
                {
                    int val = int.Parse(hex, System.Globalization.NumberStyles.HexNumber);
                    textBox5.Text += Encoding.ASCII.GetString(new byte[] { (Byte)val } );
                }
                catch (Exception ex)
                {
                    ;
                }
            }
        }

        private void textBox5_TextChanged(object sender, EventArgs e)
        {
            
        }

        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            ReceivedDataConvert();
        }

        
        
        private void serialPort1_DataReceived(object sender, System.IO.Ports.SerialDataReceivedEventArgs e)
        {

        }

        

        private void timer1_Tick(object sender, EventArgs e)
        {
            button5_Click(null, null);
        }
        private void send_mqtt_gw_cmd(TextBox tb)
        {
            string str = tb.Text;
            str += "\r\n";
            client.Publish(textBox7.Text, Encoding.UTF8.GetBytes(str), MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE, true);
        }
        private void button10_Click(object sender, EventArgs e)
        {
            send_mqtt_gw_cmd(textBox14);
        }

        private void button11_Click(object sender, EventArgs e)
        {
            send_mqtt_gw_cmd(textBox15);
        }

        private void button12_Click(object sender, EventArgs e)
        {
            send_mqtt_gw_cmd(textBox16);
        }

        private void button13_Click(object sender, EventArgs e)
        {
            send_mqtt_gw_cmd(textBox17);
        }

        private void button14_Click(object sender, EventArgs e)
        {
            send_mqtt_gw_cmd(textBox18);
        }

        private void button15_Click(object sender, EventArgs e)
        {
            send_mqtt_gw_cmd(textBox19);
        }

        private void button16_Click(object sender, EventArgs e)
        {
            send_mqtt_gw_cmd(textBox20);
        }

        private void button17_Click(object sender, EventArgs e)
        {
            send_mqtt_gw_cmd(textBox21);
        }
        private void button20_Click(object sender, EventArgs e)
        {
            //Mapping2Map(22.640629, 120.355064);

        }

        private void Mapping2Map(double lat,double longt)
        {
            gMapControl1.DragButton = MouseButtons.Right;
            gMapControl1.MapProvider = GMapProviders.GoogleMap;
            gMapControl1.ShowCenter = false;
            gMapControl1.Position = new GMap.NET.PointLatLng(lat, longt);
            gMapControl1.MinZoom = 1;//minium zoom level
            gMapControl1.MaxZoom = 100;//maxiumu zoom level
            gMapControl1.Zoom = 17;//current zoom level

            GMapMarker gMapMarker = new GMarkerGoogle(new PointLatLng(lat, longt),
            GMarkerGoogleType.green);//在(45.7，126.695）上繪製一綠色點
            GMapOverlay gMapOverlay = new GMapOverlay("mark");　　//建立圖層
            gMapOverlay.Markers.Add(gMapMarker);　　//向圖層中新增標籤
            gMapControl1.Overlays.Add(gMapOverlay);  //向控制元件中新增圖層
        }

        private void button7_Click(object sender, EventArgs e)
        {
            //AT+Door=1
            upload_mqtt("AT+Door=1");
        }

        private void button3_Click(object sender, EventArgs e)
        {
            //AT+Door=0
            upload_mqtt("AT+Door=0");
        }

        private void button8_Click(object sender, EventArgs e)
        {
            //AT+ARM
            upload_mqtt("AT+ARM");
        }

        private void button9_Click(object sender, EventArgs e)
        {
            //AT+DISARM
            upload_mqtt("AT+DISARM");
        }

        private void button18_Click(object sender, EventArgs e)
        {
            //AT+GetStatus=?
            upload_mqtt("AT+GetStatus=?");
        }
        private void upload_mqtt(string data)
        {
            client.Publish(textBox7.Text, Encoding.UTF8.GetBytes(data));
        }

        private void button19_Click(object sender, EventArgs e)
        {
            textBox2.Text = "";
        }
    }
}
