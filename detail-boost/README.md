# Detail Boost – mod giảm lag cho Geometry Dash (Geode)

Giảm lag ở các level nhiều hiệu ứng **mà không xóa / ẩn bất kỳ object nào** – block, gai, saw, decoration, trigger giữ nguyên 100%.

## Mod làm gì
| Tính năng | Tác dụng |
|---|---|
| Reduce particles + Particle amount (%) | Giới hạn số hạt (particle) của các hiệu ứng particle lớn. Mặc định còn 35%. |
| Only inside levels | Chỉ giảm particle khi đang chơi level, menu/icon kit không bị đổi. |
| Disable camera shake | Bỏ qua trigger rung màn hình. |

Hiệu ứng particle nhỏ (<= 16 hạt, ví dụ nhặt coin, bấm orb) không bị đụng tới.

## Lấy file .geode (không cần cài gì trên máy)
Mình không thể biên dịch trực tiếp trong khung chat (cần Geode SDK + trình biên dịch cho từng nền tảng), nên project đã kèm sẵn GitHub Actions: GitHub sẽ tự build ra file `.geode` cho Windows / Android 32 & 64-bit / macOS / iOS.

1. Đăng ký/đăng nhập https://github.com (miễn phí).
2. Bấm **New repository** → đặt tên `detail-boost` → **Create repository**.
3. Giải nén file zip này, rồi bấm **uploading an existing file** và kéo thả **toàn bộ** nội dung vào (nhớ cả thư mục `.github`). 
   - Nếu GitHub không nhận thư mục `.github` (thư mục ẩn): bấm **Add file → Create new file**, gõ tên `.github/workflows/build.yml`, rồi dán nội dung file `build.yml` trong zip vào.
4. Bấm **Commit changes**.
5. Vào tab **Actions** → đợi build xong (khoảng 5–15 phút, dấu tích xanh).
6. Bấm vào lần chạy vừa xong → kéo xuống mục **Artifacts** → tải **Build Output** → giải nén ra `vnuser.detail-boost.geode`.

Nếu một nền tảng báo đỏ, mở job đó và gửi mình đoạn log lỗi, mình sửa tiếp.

## Cài mod
- **Windows**: chép file `.geode` vào `Geometry Dash/geode/mods/` rồi mở game.
- **Android**: chép vào thư mục `geode/mods` của Geode Launcher (thường nằm trong `Android/media/com.geode.launcher/game/geode/mods`).
- Cần Geode loader **v5.11.1 trở lên** và GD **2.2081**.

## Đổi tên / tác giả
Mở `mod.json`, sửa `"id"` (dạng `tenban.detail-boost`) và `"developer"`.

## Lưu ý trung thực về hiệu quả
Mod này giảm tải **particle** và rung màn hình – hai thứ khá nặng ở level nhiều hiệu ứng, nhưng lag còn có thể đến từ số lượng trigger move/alpha/pulse, shader, hoặc GPU/CPU yếu. Nó giúp nhiều hay ít tùy level, **không đảm bảo** mọi level lên 60+ FPS. Muốn FPS cao hơn mức khóa 60 của game cần thêm mod mở khóa FPS riêng.
