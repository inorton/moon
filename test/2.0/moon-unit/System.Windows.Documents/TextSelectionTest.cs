//
// Unit tests for TextSelection
//
// Contact:
//   Moonlight List (moonlight-list@lists.ximian.com)
//
// Copyright (C) 2010 Novell, Inc (http://www.novell.com)
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;

using Mono.Moonlight.UnitTesting;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace MoonTest.System.Windows.Documents {

	[TestClass]
	public partial class TextSelectionTest {

		RichTextBox rtb;

		[TestInitialize]
		public void Setup ()
		{
			rtb = new RichTextBox ();
		}

		[TestMethod]
		public void Defaults ()
		{
			TextSelection ts = rtb.Selection;
			Assert.AreEqual (String.Empty, ts.Text, "Text");
			Assert.AreEqual (String.Empty, ts.Xaml, "Xaml");
		}

		[TestMethod]
		public void Text_Set_Null ()
		{
			Assert.Throws<ArgumentNullException> (delegate {
				rtb.Selection.Text = null;
			}, "null");
		}

		[TestMethod]
		[Ignore ("crash")]
		public void Text ()
		{
			TextSelection ts = rtb.Selection;
			ts.Text = "Moon";
			Assert.AreEqual (String.Empty, ts.Text, "Text"); // still empty!
		}

		[TestMethod]
		public void Xaml_Set_Null ()
		{
			Assert.Throws<ArgumentNullException> (delegate {
				rtb.Selection.Xaml = null;
			}, "null");
		}

		[TestMethod]
		[MoonlightBug ("no exception is thrown on invalid xaml")]
		public void Xaml_Set_Invalid ()
		{
			TextSelection ts = rtb.Selection;
			Assert.Throws<ArgumentException> (delegate {
				ts.Xaml = "Moon";
			}, "non-xaml");
		}
	}
}
