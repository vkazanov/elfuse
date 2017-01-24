(require 'seq)
(require 'rx)
(require 'elfuse)

(defvar list-buffers--posix-portable-filename-re
  (rx (one-or-more (in "-0-9A-Za-z._")) line-end))

(defun list-buffers--filter (buf)
  (let ((name (buffer-name buf)))
    (string-match list-buffers--posix-portable-filename-re name)))

(defun list-buffers--list-buffer-names ()
  (let ((bufs (seq-filter #'list-buffers--filter (buffer-list))))
    (seq-map #'buffer-name bufs)))

(elfuse-define-op readdir (path)
  (message "READDIR: %s" path)
  (unless (equal path "/")
    (signal 'elfuse-op-error elfuse-errno-ENOENT))
  (seq-concatenate 'vector
                   '("." "..")
                   (list-buffers--list-buffer-names)))

(elfuse-define-op getattr (path)
  (message "GETATTR: %s" path)
  (cond
   ((equal path "/")
    (vector 'dir 0))
   ((member (file-name-nondirectory path) (list-buffers--list-buffer-names))
    (vector 'file 0))
   (t (signal 'elfuse-op-error elfuse-errno-ENOENT))))
