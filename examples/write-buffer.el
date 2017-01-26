;; This file is part of Elfuse.

;; Elfuse is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; Elfuse is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with Elfuse.  If not, see <http://www.gnu.org/licenses/>.

(require 'seq)
(require 'elfuse)

(defvar write-buffer--buffer-name "*Elfuse buffer*")

(elfuse-define-op readdir (path)
  (unless (equal path "/")
    (signal 'elfuse-op-error elfuse-ENOENT))
  ["." ".." "buffer"])

(elfuse-define-op getattr (path)
  (message "GETATTR: %s" path)
  (cond ((equal path "/")
         [dir 0])
        ((equal path "/buffer")
         (vector 'file (buffer-size (write-buffer--get-buffer))))
        (t (signal 'elfuse-op-error elfuse-ENOENT))))

(elfuse-define-op read (path offset size)
  (message "READ: %s %d %d" path offset size)
  (unless (equal path "/buffer")
    (signal 'elfuse-op-error elfuse-ENOENT))
  (with-current-buffer (write-buffer--get-buffer)
    (write-buffer--substring (buffer-string) offset size)))

(elfuse-define-op write (path buffer offset)
  (message "WRITE: %s %s %d" path buffer offset)
  (unless (equal path "/buffer")
    (signal 'elfuse-op-error elfuse-ENOENT))
  (with-current-buffer (write-buffer--get-buffer)
    (goto-char offset)
    (insert buffer))
  (seq-length buffer))

(elfuse-define-op truncate (path size)
  (message "TRUNCATE: %s %d" path size)
  (unless (equal path "/buffer")
    (signal 'elfuse-op-error elfuse-ENOENT))
  (with-current-buffer (write-buffer--get-buffer)
    (let ((newbufstr (write-buffer--substring (buffer-string) 0 size)))
      (erase-buffer)
      (insert newbufstr)))
  0)

(defun write-buffer--substring (str offset size)
  (cond
   ((> offset (seq-length str)) "")
   ((> (+ offset size) (seq-length str)) (seq-subseq str offset))
   (t (seq-subseq str offset (+ offset size)))))

(defun write-buffer--get-buffer ()
  (get-buffer-create write-buffer--buffer-name))
