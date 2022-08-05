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
        void MqttServerInit()
        {
            X509Certificate caCertificate = new X509Certificate("mosquitto.org.crt");
            //X509Certificate caCertificate = X509Certificate.CreateFromCertFile("mosquitto.org.crt");
            //client = new MqttClient(textBox2.Text); //当主机地址为域名时
            client = new MqttClient(textBox2.Text, 1883, false, caCertificate, null,MqttSslProtocols.TLSv1_2);
            // 注册消息接收处理事件，还可以注册消息订阅成功、取消订阅成功、与服务器断开等事件处理函数  
            client.MqttMsgPublishReceived += client_MqttMsgPublishReceived;
            //生成客户端ID并连接服务器  
            string clientId = Guid.NewGuid().ToString();
            //string clientId = textBox3.Text;
            client.Connect(clientId);

            // 订阅主题"/home/temperature" 消息质量为 2   
            string pub1 = @"/AVIS/" + textBox3.Text + "/downlink";
            client.Subscribe(new string[] {pub1 }, new byte[] { MqttMsgBase.QOS_LEVEL_AT_MOST_ONCE });
            textBox1.Text += DateTime.Now.ToLongTimeString()+">>Connect to:" +textBox2.Text+ "\r\n";
            textBox1.Text += DateTime.Now.ToLongTimeString() + ">>listen:" + pub1+ "\r\n--------------------\r\n";
            
        }
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
            //txt_data.Text += param1.ToString()+"\r\n";
            //MessageBox.Show(param1);
            string str_uplink = @"/AVIS/" + textBox3.Text + "/uplink";
            string msg = DateTime.Now.ToLongTimeString() + ">>From:" + topic + "\r\nReceived:" + System.Text.Encoding.Default.GetString(data) + "\r\n";
            textBox1.Text += msg;

            string cmd = System.Text.Encoding.Default.GetString(data);
            
            if (String.Compare(cmd, 0, "AT+Relay=", 0, 9) == 0)
            {
                textBox8.Text = cmd.Split('=')[1];
                msg = "Change relay status:" + textBox8.Text + "\r\n";
                textBox1.Text += msg;

                string return_msg = "OK+Relay";
                client.Publish(str_uplink, Encoding.UTF8.GetBytes(return_msg), MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE, true);
                msg = DateTime.Now.ToLongTimeString() + ">>Send to:" + str_uplink + "\r\nData:" + return_msg + "\r\n";
                textBox1.Text += msg;


                msg = "--------------------\r\n";
                textBox1.Text += msg;

                button6_Click(null, null);
            }
            else if (String.Compare(cmd, 0, "AT+Door=", 0, 8) == 0)
            {
                textBox8.Text = cmd.Split('=')[1];
                msg = "Change Door status:" + textBox7.Text + "\r\n";
                textBox1.Text += msg;

                string return_msg = "OK+Door";
                client.Publish(str_uplink, Encoding.UTF8.GetBytes(return_msg), MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE, true);
                msg = DateTime.Now.ToLongTimeString() + ">>Send to:" + str_uplink + "\r\nData:" + return_msg + "\r\n";
                textBox1.Text += msg;

                msg = "--------------------\r\n";
                textBox1.Text += msg;

            }
            else if (String.Compare(cmd, 0, "AT+GetStatus=?", 0, 14) == 0)
            {
                
                string return_msg = "OK+GetStatus:"+textBox4.Text+"," + textBox5.Text + "," + textBox6.Text + "," + textBox9.Text + "," + textBox7.Text + ",";
                client.Publish(str_uplink, Encoding.UTF8.GetBytes(return_msg), MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE, true);
                msg = DateTime.Now.ToLongTimeString() + ">>Send to:" + str_uplink + "\r\nData:" + return_msg + "\r\n";
                textBox1.Text += msg;

                msg = "--------------------\r\n";
                textBox1.Text += msg;
            }
            else if (String.Compare(cmd, 0, "AT+ChangeUploadInterval=", 0, 24) == 0)
            {
                textBox10.Text = cmd.Split('=')[1];
                msg = "Change upload interval:" + textBox10.Text + "\r\n";
                textBox1.Text += msg;

                string return_msg = "OK+ChangeUploadInterval";
                client.Publish(str_uplink, Encoding.UTF8.GetBytes(return_msg), MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE, true);
                msg = DateTime.Now.ToLongTimeString() + ">>Send to:" + str_uplink + "\r\nData:" + return_msg + "\r\n";
                textBox1.Text += msg;

                msg = "--------------------\r\n";
                textBox1.Text += msg;

            }

            else
            {
                msg = "--------------------\r\n";
                textBox1.Text += msg;
            }

            
        }

        private void button1_Click(object sender, EventArgs e)
        {
            MqttServerInit();
        }

        private void button2_Click(object sender, EventArgs e)
        {
            client.Disconnect();
        }

        private void button3_Click(object sender, EventArgs e)
        {
            string str_event = @"/AVIS/" + textBox3.Text + "/EVENT";
            string data = "+EVT_ACC_ON:" + textBox4.Text + "," + textBox5.Text + "," + textBox6.Text + "," + textBox7.Text;
            client.Publish(str_event, Encoding.UTF8.GetBytes(data), MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE, true);
            string msg = DateTime.Now.ToLongTimeString() + ">>Send to:" + str_event + "\r\nData:" + data + "\r\n--------------------\r\n";
            textBox1.Text += msg;
        }

        private void button4_Click(object sender, EventArgs e)
        {
            string str_event = @"/AVIS/" + textBox3.Text + "/EVENT";
            string data = "+EVT_ACC_OFF:" + textBox4.Text + "," + textBox5.Text + "," + textBox6.Text + "," + textBox7.Text;
            client.Publish(str_event, Encoding.UTF8.GetBytes(data), MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE, true);
            string msg = DateTime.Now.ToLongTimeString() + ">>Send to:" + str_event + "\r\nData:" + data + "\r\n--------------------\r\n";
            textBox1.Text += msg;
        }

        private void button6_Click(object sender, EventArgs e)
        {
            string str_event = @"/AVIS/" + textBox3.Text + "/EVENT";
            string data = "+EVT_RELAY_CHANGE:" + textBox8.Text + "," + textBox8.Text + "," + textBox8.Text + "," + textBox8.Text;
            client.Publish(str_event, Encoding.UTF8.GetBytes(data), MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE, true);
            string msg = DateTime.Now.ToLongTimeString() + ">>Send to:" + str_event + "\r\nData:" + data + "\r\n--------------------\r\n";
            textBox1.Text += msg;
        }

        private void button5_Click(object sender, EventArgs e)
        {
            string str_event = @"/AVIS/" + textBox3.Text + "/EVENT";
            string data = "+EVT_STATUS:" + textBox4.Text + "," + textBox5.Text + "," + textBox6.Text + "," + textBox9.Text + "," + textBox7.Text;
            client.Publish(str_event, Encoding.UTF8.GetBytes(data), MqttMsgBase.QOS_LEVEL_EXACTLY_ONCE, true);
            string msg = DateTime.Now.ToLongTimeString() + ">>Send to:" + str_event + "\r\nData:" + data + "\r\n--------------------\r\n";
            textBox1.Text += msg;
        }

        private void button7_Click(object sender, EventArgs e)
        {
            textBox1.Text = "";
        }
    }
}
