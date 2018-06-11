using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Projeto_Roballo_Arduino_medidor_de_energia
{
    public partial class Medidor_de_Energia : Form
    {
        Thread X;
        public Medidor_de_Energia()
        {
            InitializeComponent();
            ComboPorta.DataSource = SerialPort.GetPortNames();
            X = new Thread(new ThreadStart(Serial));
        }
        int t = 0;
        int Q= 0;
        double P= 0.0;
        private void comboBox1_SelectedIndexChanged(object sender, EventArgs e)
        {
            
        }
            private void atualizaListaCOMs()
        {
            int i;
            bool quantDiferente;    //flag para sinalizar que a quantidade de portas mudou

            i = 0;
            quantDiferente = false;

            //se a quantidade de portas mudou
            if (ComboPorta.Items.Count == SerialPort.GetPortNames().Length)
            {
                foreach (string s in SerialPort.GetPortNames())
                {
                    if (ComboPorta.Items[i++].Equals(s) == false)
                    {
                        quantDiferente = true;
                    }
                }
            }
            else
            {
                quantDiferente = true;
            }

            //Se não foi detectado diferença
            if (quantDiferente == false)
            {
                return;                     //retorna
            }

            //limpa comboBox
            ComboPorta.Items.Clear();

            //adiciona todas as COM diponíveis na lista
            foreach (string s in SerialPort.GetPortNames())
            {
                ComboPorta.Items.Add(s);
            }
            //seleciona a primeira posição da lista
            ComboPorta.SelectedIndex = 0;
        }
        private void label4_Click(object sender, EventArgs e)
        {

        }

        private void button1_Click(object sender, EventArgs e)
        {
            if (SP.IsOpen)
            {
                SP.Close();
                X.Abort();
                timerx.Enabled = false;
            }
            else
            {
                SP.PortName = ComboPorta.Text;
                SP.BaudRate = Convert.ToInt32(ComboBaud.Text);
                SP.Open();
                //X.Start();
                timerx.Enabled = true;

            }
        }

        void Serial()
        {
            while (true)
            {
                SP.Write("a");
                textBox1.Text += SP.ReadLine();
                textBox1.Text += "\r\n";
            }
        }
        private void SP_DataReceived(object sender, System.IO.Ports.SerialDataReceivedEventArgs e)
        {
            this.Invoke(new EventHandler(transfere));
        }

        private void timer1_Tick(object sender, EventArgs e)
        {
            SP.Write("a");
            if (chart1.Series[0].Points.Count > 10 & chart2.Series[0].Points.Count > 10)
            {
                chart1.Series[0].Points.RemoveAt(0);
                chart2.Series[0].Points.RemoveAt(0);
                chart1.Update();
                chart2.Update();
            } 
            atualizaListaCOMs();

        }
       
        void transfere(object sender, EventArgs e)
        {
            string dado = SP.ReadLine();
            string[] dados = dado.Split(',');
            textBox1.Text += dados[0];
            textBox1.Text += "A \r\n";
            textBox1.Select(textBox1.Text.Length, 0);
            textBox1.ScrollToCaret();

            P = double.Parse(dados[1]);

            textBox2.Text += (P*Q/100.0);
            textBox2.Text += "W\r\n";
            textBox2.Select(textBox2.Text.Length, 0);
            textBox2.ScrollToCaret();

            if (comboTensao.SelectedItem != null)
            {
               Q = int.Parse(comboTensao.SelectedItem.ToString());
            }
            else
            { //Value is null }
            }

            //chart1.Series[0].Points.AddXY(t++, (Convert.ToDouble(dados[0]))/1000);
            chart1.Series[0].Points.Add(Convert.ToDouble(dados[0])/100);
            chart2.Series[0].Points.Add(Convert.ToDouble(dados[1])/100*Q);
            //chart2.Series[1].Points.AddXY(t++, (Convert.ToDouble(dados[1])) / 100);
            // int x = chart1.Series[0].Points.Count;
        }


        private void textBox1_TextChanged(object sender, EventArgs e)
        {

        }

        private void Form1_FormClosed(object sender, FormClosedEventArgs e)
        {
            SP.Close();
            X.Abort();
            timerx.Enabled = false;
        }

        private void Form1_Load(object sender, EventArgs e)
        {

        }

        private void chart1_Click(object sender, EventArgs e)
        {

        }

        private void checkBox1_CheckedChanged(object sender, EventArgs e)
        {
            if (checkBox1.Checked)
            {
                chart1.ChartAreas[0].Area3DStyle.Enable3D = true;
                chart2.ChartAreas[0].Area3DStyle.Enable3D = true;
            }
            else
            {
                chart1.ChartAreas[0].Area3DStyle.Enable3D = false;
                chart2.ChartAreas[0].Area3DStyle.Enable3D = false;
            }
        }
    }
}
