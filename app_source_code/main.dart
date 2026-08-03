import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'dart:async'; // để dùng Timer

void main() {
  runApp(const LockControlApp());

}

class LockControlApp extends StatelessWidget {
  const LockControlApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Điều khiển khóa cửa',
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.blue),
        useMaterial3: true,
      ),
      home: const HomePage(),
      debugShowCheckedModeBanner: false,
    );
  }
}



class HomePage extends StatefulWidget {
  const HomePage({super.key});
  @override
  State<HomePage> createState() => _HomePageState();
}



class _HomePageState extends State<HomePage> {
  final String esp32Ip = '172.20.10.3'; // thay bằng IP thật của ESP32
  String status = 'unknown';
  bool isLoading = false;
  String lockImage = 'assets/lock_closed.png'; // ảnh mặc định là khóa



  Future<void> sendRequest(String path) async {
    setState(() => isLoading = true);
    try {
      const apiKey = 'sixsixsix';
      final url = Uri.parse('http://$esp32Ip/$path?key=$apiKey');
      final res = await http.get(url).timeout(const Duration(seconds: 3));



      if (res.statusCode == 200) {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text(res.body)));



        if (path == 'unlock') {
          // Khi mở khóa → đổi ảnh sang mở
          setState(() => lockImage = 'assets/lock_open.png');
          // Sau 5 giây tự về ảnh khóa
          Timer(const Duration(seconds: 5), () {
            setState(() => lockImage = 'assets/lock_closed.png');
          });
        } else if (path == 'lock') {
          // Khi khóa → đổi ảnh về khóa
          setState(() => lockImage = 'assets/lock_closed.png');
        }

      } else {
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(const SnackBar(content: Text('Lỗi kết nối với ESP32')));
      }
    } catch (e) {
      ScaffoldMessenger.of(
        context,
      ).showSnackBar(SnackBar(content: Text('Không thể kết nối: $e')));
    } finally {
      setState(() => isLoading = false);
    }
  }

  Future<void> getStatus() async {
    try {
      const apiKey = 'sixsixsix';
      final res = await http.get(
        Uri.parse('http://$esp32Ip/status?key=$apiKey'),
      );
      if (res.statusCode == 200) {
        setState(() => status = res.body);
      }
    } catch (_) {}
  }



  @override
  void initState() {
    super.initState();
    getStatus();
  }



  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Điều khiển hệ thống khóa cửa'),
        titleTextStyle:const TextStyle(
          fontSize: 20,
          fontWeight: FontWeight.bold,
color: Colors.purpleAccent,
        ),
        centerTitle: true,
      ),
      body: Center(
        child:
            isLoading
                ? const CircularProgressIndicator()
                : Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    // 👉 Thêm ảnh dưới tiêu đề
                    Image.asset(lockImage, width: 150, height: 150),
                    const SizedBox(height: 30),
                    Text(
                      'Trạng thái hiện tại: $status',
                      style: const TextStyle(fontSize: 18),
                    ),
                    const SizedBox(height: 40),
                    ElevatedButton.icon(
                      icon: const Icon(Icons.lock),
                      label: const Text('Khóa hệ thống'),
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.redAccent,
                        foregroundColor: Colors.white,
                        minimumSize: const Size(200, 50),
                      ),
                      onPressed: () => sendRequest('lock'),
                    ),
                    const SizedBox(height: 20),
                    ElevatedButton.icon(
                      icon: const Icon(Icons.lock_open),
                      label: const Text('Mở khóa'),
                      style: ElevatedButton.styleFrom(
                        backgroundColor: Colors.green,
                        foregroundColor: Colors.white,
                        minimumSize: const Size(200, 50),
                      ),
                      onPressed: () => sendRequest('unlock'),
                    ),
                    const SizedBox(height: 40),
                    ElevatedButton.icon(
                      icon: const Icon(Icons.history),
                      label: const Text('Xem lịch sử'),
                      style: ElevatedButton.styleFrom(
                        minimumSize: const Size(200, 50),
                      ),
                      onPressed: () {
                        Navigator.push(
                          context,
                          MaterialPageRoute(
                            builder: (context) => HistoryPage(esp32Ip: esp32Ip),
                          ),
                        );
                      },
                    ),
                  ],
                ),
      ),
    );
  }
}



class HistoryPage extends StatefulWidget {
  final String esp32Ip;
  const HistoryPage({super.key, required this.esp32Ip});
  @override
  State<HistoryPage> createState() => _HistoryPageState();
}



class _HistoryPageState extends State<HistoryPage> {
  String logs = 'Đang tải...';
  Future<void> loadHistory() async {
    try {
      const apiKey = 'sixsixsix';
      final res = await http
      .get(Uri.parse('http://${widget.esp32Ip}/logs?key=$apiKey'))
          .timeout(const Duration(seconds: 3));
      if (res.statusCode == 200) {
        setState(() => logs = res.body);
      } else {
        setState(() => logs = 'Không thể tải lịch sử');
      }
    } catch (e) {
      setState(() => logs = 'Lỗi: $e');
    }
  }

  @override
  void initState() {
    super.initState();
    loadHistory();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Lịch sử hoạt động')),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: SingleChildScrollView(
          child: Text(logs, style: const TextStyle(fontSize: 16)),
        ),
      ),
    );
  }
} 
